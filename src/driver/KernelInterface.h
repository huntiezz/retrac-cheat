#include "Includes/Structures.h"

namespace Memory {
    class Driver {
    public:
        uintptr_t Base;
        uintptr_t Cr3;
        uintptr_t Peb;

        HANDLE Handle;
        HANDLE DriverHandle = INVALID_HANDLE_VALUE;
        int ProcessId;
        DWORD AttachLastError = 0;
        DWORD DriverConnectLastError = 0;
        DWORD DriverReadLastError = 0;
        uintptr_t DriverReadLastTargetVa = 0;
        // Set when IOCTL fails after a successful kernel read elsewhere (target VA/phys translate, not transport).
        bool DriverReadLastFailureTargetInaccessible = false;
        bool ReadOnlyAttach = false;
        bool KernelAttached = false;

        bool ConnectKernelDriver();
        void DisconnectKernelDriver();

        bool Initialize();
        bool IsValid( uint64_t Addr );

        bool Read( uintptr_t Addr, PVOID Buffer, DWORD Size );
        bool ReadUnchecked( uintptr_t Addr, PVOID Buffer, DWORD Size );

        uintptr_t GetBase( int Pid );

    private:
        bool ReadViaKernel( uintptr_t Addr, PVOID Buffer, DWORD Size );
        uintptr_t GetBaseViaKernel( int Pid, bool requireAttached = true );

    public:

        DWORD GetPid( LPCTSTR ProcName );
        DWORD RetrieveDtb( int Pid );

        template <typename T>
        T ReadRequest( uintptr_t Addr ) {
            T Tmp{};
            Read( Addr, &Tmp, sizeof( T ) );
            return Tmp;
        }

        template <typename T>
        bool ReadRequestOk( uintptr_t Addr, T& out ) {
            return Read( Addr, &out, sizeof( T ) );
        }

        template <typename T>
        bool WriteRequest( uintptr_t Addr, const T& Val ) {
            SIZE_T written = 0;
            return WriteProcessMemory(
                Handle,
                reinterpret_cast<LPVOID>( Addr ),
                &Val,
                sizeof( T ),
                &written
            ) == TRUE && written == sizeof( T );
        }
    };
    inline Driver Process;
}
