#include "KernelInterface.h"
#include "DriverConfig.h"
#include "../util/obfuscate.h"
#include <iostream>
#include <cstring>
#include <vector>

namespace Memory {

    namespace {
        void TryEnableSeDebugPrivilege( ) {
            HANDLE token = nullptr;
            if ( !OpenProcessToken( GetCurrentProcess( ), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token ) ) {
                std::cout << xorstr_( "[-] SeDebugPrivilege: OpenProcessToken failed (GetLastError=" )
                          << GetLastError( ) << xorstr_( ")\n" );
                return;
            }

            TOKEN_PRIVILEGES tp{ };
            tp.PrivilegeCount = 1;
            tp.Privileges[ 0 ].Attributes = SE_PRIVILEGE_ENABLED;
            if ( !LookupPrivilegeValueW( nullptr, SE_DEBUG_NAME, &tp.Privileges[ 0 ].Luid ) ) {
                const DWORD err = GetLastError( );
                CloseHandle( token );
                std::cout << xorstr_( "[-] SeDebugPrivilege: LookupPrivilegeValue failed (GetLastError=" )
                          << err << xorstr_( ")\n" );
                return;
            }

            if ( !AdjustTokenPrivileges( token, FALSE, &tp, sizeof( tp ), nullptr, nullptr ) ) {
                const DWORD err = GetLastError( );
                CloseHandle( token );
                std::cout << xorstr_( "[-] SeDebugPrivilege: AdjustTokenPrivileges failed (GetLastError=" )
                          << err << xorstr_( ")\n" );
                return;
            }

            if ( GetLastError( ) == ERROR_NOT_ALL_ASSIGNED ) {
                std::cout << xorstr_( "[-] SeDebugPrivilege: not held by token (GetLastError=" )
                          << ERROR_NOT_ALL_ASSIGNED << xorstr_( ")\n" );
            }

            CloseHandle( token );
        }

        bool IoctlBuffered( HANDLE device, DWORD code, void* buffer, DWORD size, DWORD* outReturned = nullptr ) {
            DWORD returned = 0;
            const BOOL ok = DeviceIoControl(
                                device,
                                code,
                                buffer,
                                size,
                                buffer,
                                size,
                                &returned,
                                nullptr
                            );
            if ( outReturned )
                *outReturned = returned;
            return ok != FALSE;
        }

        // Driver handler copies read bytes to m_buffer (user VA), not into the METHOD_BUFFERED struct tail.
        bool KernelReadIoctl( HANDLE device, uint32_t pid, uint64_t address, void* staging,
                              uint64_t stagingVa, DWORD size ) {
            DriverReadRequest req{ };
            req.m_pid = pid;
            req.m_address = address;
            req.m_buffer = stagingVa;
            req.m_size = size;
            return IoctlBuffered(
                       device,
                       IOCTL_ReadMemory,
                       &req,
                       static_cast<DWORD>( sizeof( req ) )
                   );
        }
    }

    void Driver::DisconnectKernelDriver( ) {
        KernelAttached = false;
        if ( DriverHandle != INVALID_HANDLE_VALUE ) {
            CloseHandle( DriverHandle );
            DriverHandle = INVALID_HANDLE_VALUE;
        }
    }

    bool Driver::ReadViaKernel( uintptr_t Addr, PVOID Buffer, DWORD Size ) {
        DriverReadLastError = 0;
        DriverReadLastTargetVa = Addr;
        DriverReadLastFailureTargetInaccessible = false;
        if ( !KernelAttached || DriverHandle == INVALID_HANDLE_VALUE || !ProcessId || !Buffer
             || !Size || Size > 0x10000 ) {
            return false;
        }

        const uint32_t pid = static_cast<uint32_t>( ProcessId );
        const uint64_t target = static_cast<uint64_t>( Addr );

        alignas( 8 ) UCHAR stackStaging[ 512 ];
        std::vector<UCHAR> heapStaging;
        void* staging = stackStaging;
        if ( Size > sizeof( stackStaging ) ) {
            heapStaging.resize( Size );
            staging = heapStaging.data( );
        }

        const uint64_t stagingVa = reinterpret_cast<uint64_t>( staging );
        if ( !KernelReadIoctl( DriverHandle, pid, target, staging, stagingVa, Size ) ) {
            DriverReadLastError = GetLastError( );
            if ( Base && DriverReadLastError == ERROR_GEN_FAILURE ) {
                uint16_t probeMz{};
                if ( KernelReadIoctl(
                         DriverHandle,
                         pid,
                         static_cast<uint64_t>( Base ),
                         &probeMz,
                         reinterpret_cast<uint64_t>( &probeMz ),
                         sizeof( probeMz )
                     )
                     && probeMz == 0x5A4D ) {
                    DriverReadLastFailureTargetInaccessible = true;
                }
            }
            return false;
        }

        std::memcpy( Buffer, staging, Size );
        return true;
    }

    uintptr_t Driver::GetBaseViaKernel( int Pid, bool requireAttached ) {
        if ( requireAttached && !KernelAttached ) {
            return 0;
        }
        if ( DriverHandle == INVALID_HANDLE_VALUE || !Pid ) {
            return 0;
        }

        uint64_t base = 0;
        DriverGetBaseRequest req{ };
        req.m_pid = static_cast<uint32_t>( Pid );
        req.m_address = &base;

        if ( !IoctlBuffered(
                 DriverHandle,
                 IOCTL_GetBaseAddress,
                 &req,
                 static_cast<DWORD>( sizeof( req ) )
             ) ) {
            return 0;
        }

        return static_cast<uintptr_t>( base );
    }

    bool Driver::ConnectKernelDriver( ) {
        DriverConnectLastError = 0;
        if ( KernelAttached ) {
            return true;
        }

        if ( DriverHandle != INVALID_HANDLE_VALUE ) {
            CloseHandle( DriverHandle );
            DriverHandle = INVALID_HANDLE_VALUE;
        }

        DriverHandle = CreateFileW(
            RETRAC_DRIVER_DEVICE_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if ( DriverHandle == INVALID_HANDLE_VALUE ) {
            DriverConnectLastError = GetLastError( );
            return false;
        }

        if ( !ProcessId ) {
            DriverConnectLastError = ERROR_INVALID_PARAMETER;
            DisconnectKernelDriver( );
            return false;
        }

        const uintptr_t probeBase = GetBaseViaKernel( ProcessId, false );
        if ( !probeBase ) {
            DriverConnectLastError = GetLastError( );
            DisconnectKernelDriver( );
            return false;
        }

        uint16_t magic{};
        if ( !KernelReadIoctl(
                 DriverHandle,
                 static_cast<uint32_t>( ProcessId ),
                 static_cast<uint64_t>( probeBase ),
                 &magic,
                 reinterpret_cast<uint64_t>( &magic ),
                 sizeof( magic )
             )
             || magic != 0x5A4D ) {
            DriverConnectLastError = ERROR_GEN_FAILURE;
            DisconnectKernelDriver( );
            return false;
        }

        KernelAttached = true;
        Base = probeBase;
        return true;
    }

    bool Driver::Initialize( ) {
        AttachLastError = 0;
        ReadOnlyAttach = false;

        if ( !ProcessId ) {
            return false;
        }

        if ( Handle ) {
            CloseHandle( Handle );
            Handle = nullptr;
        }

        TryEnableSeDebugPrivilege( );

        constexpr DWORD full_access =
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION;

        Handle = OpenProcess( full_access, FALSE, static_cast<DWORD>( ProcessId ) );
        if ( Handle ) {
            return true;
        }

        AttachLastError = GetLastError( );

        Handle = OpenProcess(
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            FALSE,
            static_cast<DWORD>( ProcessId )
        );
        if ( Handle ) {
            ReadOnlyAttach = true;
            AttachLastError = 0;
            return true;
        }

        AttachLastError = GetLastError( );
        Handle = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
            FALSE,
            static_cast<DWORD>( ProcessId )
        );
        if ( Handle ) {
            ReadOnlyAttach = true;
            AttachLastError = 0;
            return true;
        }

        AttachLastError = GetLastError( );
        return false;
    }

    bool Driver::ReadUnchecked( uintptr_t Addr, PVOID Buffer, DWORD Size ) {
        if ( !Buffer || !Size ) {
            return false;
        }
        if ( KernelAttached ) {
            return ReadViaKernel( Addr, Buffer, Size );
        }
        if ( !Handle ) {
            return false;
        }
        SIZE_T read = 0;
        return ReadProcessMemory(
                   Handle,
                   reinterpret_cast<LPCVOID>( Addr ),
                   Buffer,
                   Size,
                   &read
               ) != FALSE;
    }

    bool Driver::Read( uintptr_t Addr, PVOID Buffer, DWORD Size ) {
        if ( !Buffer || !Size ) {
            return false;
        }
        if ( KernelAttached ) {
            return ReadViaKernel( Addr, Buffer, Size );
        }
        if ( !Handle ) {
            return false;
        }
        SIZE_T read = 0;
        if ( ReadProcessMemory(
                 Handle,
                 reinterpret_cast<LPCVOID>( Addr ),
                 Buffer,
                 Size,
                 &read
             ) == FALSE ) {
            return false;
        }
        return read == Size;
    }

    bool Driver::IsValid( uint64_t Addr ) {
        return Addr > 0x1000 && Addr < 0x7FFFFFFFFFFF;
    }

    uintptr_t Driver::GetBase( int Pid ) {
        if ( KernelAttached ) {
            const uintptr_t kernelBase = GetBaseViaKernel( Pid );
            if ( kernelBase ) {
                return kernelBase;
            }
        }

        if ( !Pid ) {
            return 0;
        }

        const HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, static_cast<DWORD>( Pid ) );
        if ( snap == INVALID_HANDLE_VALUE ) {
            return 0;
        }

        MODULEENTRY32W entry{ };
        entry.dwSize = sizeof( entry );
        uintptr_t base = 0;

        if ( Module32FirstW( snap, &entry ) ) {
            do {
                if ( !_wcsicmp( entry.szModule, L"FortniteClient-Win64-Shipping.exe" ) ) {
                    base = reinterpret_cast<uintptr_t>( entry.modBaseAddr );
                    break;
                }
            } while ( Module32NextW( snap, &entry ) );
        }

        CloseHandle( snap );
        return base;
    }

    DWORD Driver::GetPid( LPCTSTR ProcName ) {
        PROCESSENTRY32 Entry{ };
        HANDLE Snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

        if ( Snap == INVALID_HANDLE_VALUE ) {
            return 0;
        }

        Entry.dwSize = sizeof( Entry );

        if ( Process32First( Snap, &Entry ) ) {
            do {
                if ( !lstrcmpi( Entry.szExeFile, ProcName ) ) {
                    CloseHandle( Snap );
                    return Entry.th32ProcessID;
                }
            } while ( Process32Next( Snap, &Entry ) );
        }

        CloseHandle( Snap );
        return 0;
    }

    DWORD Driver::RetrieveDtb( int Pid ) {
        (void)Pid;
        return 0;
    }
}
