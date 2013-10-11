#ifdef BUILTIN_SIM

        .globl SafeFetch32, Fetch32PFI, Fetch32Resume
        .align  16
        .type   SafeFetch32,@function
        // Prototype: int SafeFetch32 (int * Adr, int ErrValue) 
SafeFetch32:
        movl    %esi, %eax
Fetch32PFI:
        movl    (%rdi), %eax
Fetch32Resume:
        ret

        .globl SafeFetchN, FetchNPFI, FetchNResume
        .align  16
        .type   SafeFetchN,@function
        // Prototype: intptr_t SafeFetchN (intptr_t * Adr, intptr_t ErrValue) 
SafeFetchN:
        movq    %rsi, %rax
FetchNPFI:
        movq    (%rdi), %rax
FetchNResume:
        ret

#endif
