format ELF64
public insertion_sort_fasm
public quicksort_fasm
public quicksort_two_threads_fasm
public dual_pivot_quicksort_fasm
public merge_sort_fasm
public heap_sort_fasm
public quicksort_branchless_fasm
public radix_sort_fasm
public bitonic_sort_fasm
public introsort_fasm
public timsort_fasm
public pdqsort_fasm
public triple_pivot_quicksort_fasm

public quickselect_fasm
public partial_sort_fasm
public cs_sort_fasm
public cs3_opt_fasm
public cs3_gs_sort_fasm
public cs_sort_u64_fasm
public cs3_gs_sort_u64_fasm

extrn cs3_heap_sort_u64_c
extrn cs3_gs_make_thresholds_u64
extrn pthread_create
extrn pthread_join
extrn cs3_gs_make_thresholds

section '.text' executable align 32

; ==============================================================================
; TRIPLE-PIVOT QUICKSORT (Разделение на 4 части)
; ==============================================================================
align 16
triple_pivot_quicksort_fasm:
    cmp esi, edx
    jge .end_tp

    ; Introspective budget: 2*floor(log2(count)), matching the C twin.
    ; Without median sampling and a depth budget, sorted input chooses the
    ; second element as p2 and recurses on N-2 elements until the stack fails.
    mov eax, edx
    sub eax, esi
    inc eax
    xor ecx, ecx
.tp_depth_loop:
    cmp eax, 1
    jle .tp_depth_ready
    shr eax, 1
    add ecx, 2
    jmp .tp_depth_loop
.tp_depth_ready:
    jmp triple_pivot_inner
.end_tp:
    ret

align 16
triple_pivot_inner:
    ; Recursive partitions can legitimately be empty (low > high), or contain
    ; a single element. Mirror the C implementation's base case BEFORE any
    ; index arithmetic; otherwise high-low underflows and dereferences wild memory.
    cmp rsi, rdx
    jge .tp_return_early

    push r12
    push r13
    push r14
    push r15
    push rbx
    ; Five saved registers plus 80 local bytes preserve 16-byte call alignment.
    sub rsp, 80

    mov r12, rdi        ; arr
    mov r13, rsi        ; low
    mov r14, rdx        ; high
    mov [rsp+64], rcx   ; remaining introsort depth budget

    mov [rsp+24], r13
    mov [rsp+32], r14

    mov rax, r14
    sub rax, r13
    cmp qword [rsp+64], 0
    jle .tp_fallback_heap
    cmp rax, 23                 ; match C INSERTION_THRESHOLD = 24
    jg .tp_pivot_sample

    mov r8, r13
    inc r8
.tp_small_outer:
    cmp r8, r14
    jg .exit_tp
    mov eax, dword [r12 + r8*4]
    mov r9, r8
    dec r9
.tp_small_inner:
    cmp r9, r13
    jl .tp_small_insert
    mov r10d, dword [r12 + r9*4]
    cmp r10d, eax
    jle .tp_small_insert
    mov dword [r12 + r9*4 + 4], r10d
    dec r9
    jmp .tp_small_inner
.tp_small_insert:
    mov dword [r12 + r9*4 + 4], eax
    inc r8
    jmp .tp_small_outer

.tp_pivot_sample:
    ; Match sorts_c.c: move the midpoint sample to low+1 before selecting
    ; p1/p2/p3. On already sorted data, using arr[low+1] directly as p2
    ; creates a (N-2) recursion and eventually overflows the stack.
    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    lea r8, [r13 + 1]
    mov r9d, dword [r12 + rax*4]
    mov r10d, dword [r12 + r8*4]
    mov dword [r12 + r8*4], r9d
    mov dword [r12 + rax*4], r10d

.tp_partition:
    lea r8, [r13 + 1]
    mov eax, dword [r12 + r13*4]
    mov r9d, dword [r12 + r8*4]
    mov r10d, dword [r12 + r14*4]

    cmp eax, r9d
    jle .tp_s1
    mov dword [r12 + r13*4], r9d
    mov dword [r12 + r8*4], eax
    xchg eax, r9d
.tp_s1:
    cmp eax, r10d
    jle .tp_s2
    mov dword [r12 + r13*4], r10d
    mov dword [r12 + r14*4], eax
    xchg eax, r10d
.tp_s2:
    cmp r9d, r10d
    jle .tp_s3
    mov dword [r12 + r8*4], r10d
    mov dword [r12 + r14*4], r9d
    xchg r9d, r10d
.tp_s3:

    mov [rsp+0], eax
    mov [rsp+8], r9d
    mov [rsp+16], r10d

    ; IMPORTANT: install the sorted pivot samples in the sentinel slots.
    ; The final pivot-rotation below assumes arr[low]=p1,
    ; arr[low+1]=p2 and arr[high]=p3.
    mov dword [r12 + r13*4], eax
    mov dword [r12 + r8*4], r9d
    mov dword [r12 + r14*4], r10d

    ; Duplicate sample pivots can make the 4-way partition degenerate.
    cmp eax, r9d
    je .tp_fallback_heap
    cmp r9d, r10d
    je .tp_fallback_heap

    lea rsi, [r13 + 2]
    mov rdx, rsi
    mov rcx, rsi
    lea rbx, [r14 - 1]

    align 16
.tp_while_c:
    cmp rcx, rbx
    jg .tp_while_done

    mov r15d, dword [r12 + rcx*4]

    mov eax, [rsp+0]
    cmp r15d, eax
    jl .tp_case_1

    mov eax, [rsp+8]
    cmp r15d, eax
    jle .tp_case_2

    mov eax, [rsp+16]
    cmp r15d, eax
    jg .tp_case_4

    inc rcx
    jmp .tp_while_c

.tp_case_1:
    mov r8d, dword [r12 + rdx*4]
    mov dword [r12 + rcx*4], r8d
    mov dword [r12 + rdx*4], r15d

    mov r9d, dword [r12 + rsi*4]
    mov dword [r12 + rdx*4], r9d
    mov dword [r12 + rsi*4], r15d

    inc rsi
    inc rdx
    inc rcx
    jmp .tp_while_c

.tp_case_2:
    mov r8d, dword [r12 + rdx*4]
    mov dword [r12 + rcx*4], r8d
    mov dword [r12 + rdx*4], r15d

    inc rdx
    inc rcx
    jmp .tp_while_c

.tp_case_4:
    align 16
.tp_while_d:
    cmp rcx, rbx
    jge .tp_swap_d
    mov eax, dword [r12 + rbx*4]
    mov r8d, [rsp+16]
    cmp eax, r8d
    jle .tp_swap_d
    dec rbx
    jmp .tp_while_d

.tp_swap_d:
    cmp rcx, rbx
    jg .tp_while_done

    mov eax, dword [r12 + rbx*4]
    mov dword [r12 + rcx*4], eax
    mov dword [r12 + rbx*4], r15d
    dec rbx
    jmp .tp_while_c

.tp_while_done:
    mov r13, [rsp+24]
    mov r14, [rsp+32]

    lea r8, [r13 + 1]    ; low+1
    lea r10, [rsi - 1]   ; a-1
    lea r9, [rdx - 1]    ; b-1

    ; Кольцевой сдвиг для p2
    mov eax, dword [r12 + r8*4]
    mov ecx, dword [r12 + r10*4]
    mov dword [r12 + r8*4], ecx
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + r10*4], ecx
    mov dword [r12 + r9*4], eax

    ; Сдвиг для p1
    lea r11, [rsi - 2]   ; a-2
    mov eax, dword [r12 + r13*4]
    mov ecx, dword [r12 + r11*4]
    mov dword [r12 + r13*4], ecx
    mov dword [r12 + r11*4], eax

    ; Сдвиг для p3
    lea r9, [rbx + 1]    ; d+1
    mov eax, dword [r12 + r14*4]
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + r14*4], ecx
    mov dword [r12 + r9*4], eax

    mov [rsp+40], rsi
    mov [rsp+48], rdx
    mov [rsp+56], rbx

    mov rdi, r12
    mov rsi, r13
    mov rdx, [rsp+40]
    sub rdx, 3
    mov ecx, dword [rsp+64]
    dec ecx
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+40]
    dec rsi
    mov rdx, [rsp+48]
    sub rdx, 2
    mov ecx, dword [rsp+64]
    dec ecx
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+48]
    mov rdx, [rsp+56]
    mov ecx, dword [rsp+64]
    dec ecx
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+56]
    add rsi, 2
    mov rdx, [rsp+32]
    mov ecx, dword [rsp+64]
    dec ecx
    call triple_pivot_inner
    jmp .exit_tp

.tp_fallback_heap:
    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm

.exit_tp:
    add rsp, 80
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    ret

.tp_return_early:
    ret

; ==============================================================================
; INSERTION SORT
; ==============================================================================
align 16
insertion_sort_fasm:
    cmp esi, 1
    jle .end_insertion
    mov r8, 1
    align 16
.outer_loop:
    cmp r8d, esi
    jge .end_insertion
    mov eax, dword [rdi + r8*4]
    mov r9, r8
    dec r9
    align 16
.inner_loop:
    cmp r9, 0
    jl .insert_key
    mov r10d, dword [rdi + r9*4]
    cmp r10d, eax
    jle .insert_key
    mov dword [rdi + r9*4 + 4], r10d
    dec r9
    jmp .inner_loop
.insert_key:
    mov dword [rdi + r9*4 + 4], eax
    inc r8
    jmp .outer_loop
.end_insertion:
    ret

; ==============================================================================
; QUICKSORT — introspective Hoare partition
; SysV AMD64: quicksort_fasm(int *arr, int low, int high)
; ==============================================================================

align 16
quicksort_fasm:
    cmp esi, edx
    jge .qs_public_ret
    test esi, esi
    js .qs_public_ret

    ; depth_limit = 2 * floor(log2(high-low+1))
    movsxd r8, edx
    movsxd r9, esi
    sub r8, r9
    inc r8

    xor ecx, ecx
.qs_depth_init:
    cmp r8, 1
    jle .qs_depth_ready
    shr r8, 1
    add ecx, 2
    jmp .qs_depth_init

.qs_depth_ready:
    movsxd rsi, esi
    movsxd rdx, edx

    ; Align stack before CALL.
    sub rsp, 8
    call quicksort_inner
    add rsp, 8

.qs_public_ret:
    ret


align 16
quicksort_inner:
    cmp rsi, rdx
    jge .qs_inner_early_ret
    test rsi, rsi
    js .qs_inner_early_ret

    push r12
    push r13
    push r14
    push r15
    sub rsp, 24

    mov r12, rdi
    movsxd r13, esi
    movsxd r14, edx
    mov [rsp], rcx              ; remaining depth budget

.qs_loop:
    cmp r13, r14
    jge .qs_exit

    mov rax, r14
    sub rax, r13
    inc rax                     ; n = high-low+1

    ; Small partitions: insertion sort.
    cmp rax, 24
    jle .qs_insertion

    ; Worst-case protection.
    cmp qword [rsp], 0
    jle .qs_heap_fallback
    dec qword [rsp]

    ; ------------------------------------------------------------------
    ; Pivot = median of elements at quarter, middle, three-quarter.
    ; Much more robust for PipeOrgan than choosing only the midpoint.
    ; ------------------------------------------------------------------
    mov rcx, rax
    shr rcx, 2
    add rcx, r13
    mov r10d, dword [r12 + rcx*4]

    mov rcx, rax
    shr rcx, 1
    add rcx, r13
    mov r11d, dword [r12 + rcx*4]

    lea rcx, [rax + rax*2]
    shr rcx, 2
    add rcx, r13
    mov eax, dword [r12 + rcx*4]

    ; Sort the three samples; median ends in r11d.
    cmp r10d, r11d
    jle .qs_ab_sorted
    xchg r10d, r11d

.qs_ab_sorted:
    cmp r11d, eax
    jle .qs_bc_sorted
    xchg r11d, eax

.qs_bc_sorted:
    cmp r10d, r11d
    jle .qs_pivot_ready
    xchg r10d, r11d

.qs_pivot_ready:
    mov r15d, r11d

    ; ------------------------------------------------------------------
    ; Hoare partition.
    ; ------------------------------------------------------------------
    mov r8, r13
    dec r8
    mov r9, r14
    inc r9

.qs_partition_loop:
.qs_scan_left:
    inc r8
    mov eax, dword [r12 + r8*4]
    cmp eax, r15d
    jl .qs_scan_left

.qs_scan_right:
    dec r9
    mov eax, dword [r12 + r9*4]
    cmp eax, r15d
    jg .qs_scan_right

    cmp r8, r9
    jge .qs_partition_done

    mov eax, dword [r12 + r8*4]
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + r8*4], ecx
    mov dword [r12 + r9*4], eax
    jmp .qs_partition_loop

.qs_partition_done:
    ; Defensive fallback if a partition boundary is invalid or degenerate.
    cmp r9, r13
    jl .qs_heap_fallback
    cmp r9, r14
    jge .qs_heap_fallback

    mov [rsp+8], r9             ; partition boundary j

    ; left size = j-low+1
    mov rax, r9
    sub rax, r13
    inc rax

    ; right size = high-j
    mov rcx, r14
    sub rcx, r9

    ; Recurse into the smaller side; iterate over the larger one.
    ; This bounds call-stack growth even on unbalanced partitions.
    cmp rax, rcx
    jg .qs_recurse_right

.qs_recurse_left:
    mov rdi, r12
    mov rsi, r13
    mov rdx, [rsp+8]
    mov rcx, [rsp]
    call quicksort_inner

    mov r13, [rsp+8]
    inc r13
    jmp .qs_loop

.qs_recurse_right:
    mov rdi, r12
    mov rsi, [rsp+8]
    inc rsi
    mov rdx, r14
    mov rcx, [rsp]
    call quicksort_inner

    mov r14, [rsp+8]
    jmp .qs_loop

.qs_heap_fallback:
    ; heap_sort_fasm(arr + low, count)
    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .qs_exit

.qs_insertion:
    mov r8, r13
    inc r8

.qs_ins_outer:
    cmp r8, r14
    jg .qs_exit

    mov eax, dword [r12 + r8*4]
    mov r9, r8
    dec r9

.qs_ins_inner:
    cmp r9, r13
    jl .qs_ins_place

    mov ecx, dword [r12 + r9*4]
    cmp ecx, eax
    jle .qs_ins_place

    lea r10, [r9+1]
    mov dword [r12 + r10*4], ecx
    dec r9
    jmp .qs_ins_inner

.qs_ins_place:
    lea r10, [r9+1]
    mov dword [r12 + r10*4], eax
    inc r8
    jmp .qs_ins_outer

.qs_exit:
    add rsp, 24
    pop r15
    pop r14
    pop r13
    pop r12
    ret

.qs_inner_early_ret:
    ret
; ==============================================================================
; QUICKSORT BRANCHLESS (LOMUTO + ALIGNMENT)
; ==============================================================================
align 16
quicksort_branchless_fasm:
    ; The old Lomuto routine can recurse O(n) deep on PipeOrgan / duplicate
    ; patterns and overflow the stack. Route this public entry point to the
    ; guarded quicksort implementation until a genuinely branchless introsort
    ; is implemented and tested.
    jmp quicksort_fasm

align 16
quicksort_branchless_inner:
    cmp rsi, rdx
    jge .ret_inner

    push r12
    push r13
    push r14
    push r15
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov rax, r13
    add rax, r14
    shr rax, 1

    mov ecx, dword [r12 + rax*4]
    mov r10d, dword [r12 + r14*4]
    mov dword [r12 + rax*4], r10d
    mov dword [r12 + r14*4], ecx
    mov r15d, ecx

    mov r8, r13
    dec r8
    mov r9, r13
    align 16
.partition_loop:
    cmp r9, r14
    jge .partition_done

    mov eax, dword [r12 + r9*4]
    cmp eax, r15d
    setl cl
    movzx rcx, cl
    lea r10, [r8 + rcx]
    mov r11, r9
    cmovl r11, r10

    mov ecx, dword [r12 + r11*4]
    mov dword [r12 + r11*4], eax
    mov dword [r12 + r9*4], ecx
    mov r8, r10
    inc r9
    jmp .partition_loop
.partition_done:
    inc r8
    mov eax, dword [r12 + r8*4]
    mov ecx, dword [r12 + r14*4]
    mov dword [r12 + r8*4], ecx
    mov dword [r12 + r14*4], eax

    mov rdi, r12
    mov rsi, r13
    mov rdx, r8
    dec rdx
    call quicksort_branchless_inner

    mov rdi, r12
    mov rsi, r8
    inc rsi
    mov rdx, r14
    call quicksort_branchless_inner

    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
.ret_inner:
    ret

; ==============================================================================
; Двухпоточный Quicksort
; ==============================================================================
align 16
partition_first_fasm:
    mov ecx, dword [rdi + rsi*4]
    mov r8, rsi
    mov r9, rdx
    inc r9

    align 16
.while_i:
    inc r8
    cmp r8, rdx
    jg .while_j
    cmp dword [rdi + r8*4], ecx
    jl .while_i

    align 16
.while_j:
    dec r9
    cmp dword [rdi + r9*4], ecx
    jg .while_j

    cmp r8, r9
    jge .done

    mov eax, dword [rdi + r8*4]
    mov r10d, dword [rdi + r9*4]
    mov dword [rdi + r8*4], r10d
    mov dword [rdi + r9*4], eax
    jmp .while_i
.done:
    mov eax, dword [rdi + rsi*4]
    mov r10d, dword [rdi + r9*4]
    mov dword [rdi + rsi*4], r10d
    mov dword [rdi + r9*4], eax
    mov rax, r9
    ret

align 16
thread_routine_fasm:
    push rbp
    mov rbp, rsp
    mov r8, rdi
    mov rdi, [r8]
    movsxd rsi, dword [r8+8]
    movsxd rdx, dword [r8+12]
    call quicksort_fasm
    xor rax, rax
    pop rbp
    ret

align 16
quicksort_two_threads_fasm:
    cmp esi, 1
    jle .end_2thread
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15

    ; Выделение памяти под аргументы
    sub rsp, 48
    and rsp, -16

    mov r12, rdi
    mov r13d, esi

    mov rsi, 0
    mov rdx, r13
    dec rdx
    call partition_first_fasm
    mov r14, rax

    mov [rsp+8], r12
    mov dword [rsp+16], 0
    lea eax, [r14 - 1]
    mov dword [rsp+20], eax

    lea rdi, [rsp]
    xor rsi, rsi

    call .get_rip
.get_rip:
    pop rdx
    add rdx, thread_routine_fasm - .get_rip
    lea rcx, [rsp+8]
    call pthread_create
    test eax, eax
    jnz .thread_create_failed

    mov rdi, r12
    lea rsi, [r14 + 1]
    lea rdx, [r13 - 1]
    call quicksort_fasm

    mov rdi, [rsp]
    xor rsi, rsi
    call pthread_join
    jmp .thread_join_done

.thread_create_failed:
    ; If the OS cannot create the worker, sort both partitions serially.
    mov rdi, r12
    xor esi, esi
    lea edx, [r14 - 1]
    call quicksort_fasm
    mov rdi, r12
    lea rsi, [r14 + 1]
    lea rdx, [r13 - 1]
    call quicksort_fasm

.thread_join_done:
    ; корректное восстановление кадра
    lea rsp, [rbp - 32]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
.end_2thread:
    ret


; ==============================================================================
; DUAL-PIVOT QUICKSORT методом Ярославского
; ==============================================================================
align 16
dual_pivot_quicksort_fasm:
    cmp esi, edx
    jge .end_dp
    push r12
    push r13
    push r14
    push r15
    push rbx
    ; Entry RSP is 8 mod 16; five pushes align it for the internal call.
    call dual_pivot_inner
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.end_dp:
    ret

align 16
dual_pivot_inner:
    cmp rsi, rdx
    jge .ret_dp_early

    ; This routine is recursive and uses callee-saved registers. Every frame
    ; must preserve its caller's r12-r15/rbx; otherwise the child recursion
    ; corrupts the parent's base pointer, bounds, and pivot boundary (the
    ; observed large-N FASM crash after QuickSort_2Threads_FASM).
    push r12
    push r13
    push r14
    push r15
    push rbx
    ; Reserve 16 bytes: keep the stack 16-byte aligned before every CALL and
    ; retain r8 (the right partition boundary) across recursive invocations.
    sub rsp, 16

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov eax, dword [r12 + r13*4]
    mov ebx, dword [r12 + r14*4]
    cmp eax, ebx
    jle .no_swap_init
    mov dword [r12 + r13*4], ebx
    mov dword [r12 + r14*4], eax
    xchg eax, ebx
.no_swap_init:
    lea r15, [r13 + 1]
    lea r8, [r14 - 1]
    mov r9, r15

    align 16
.while_k:
    cmp r9, r8
    jg .done_while

    mov ecx, dword [r12 + r9*4]
    cmp ecx, eax
    jge .check_p2

    mov edx, dword [r12 + r15*4]
    mov dword [r12 + r15*4], ecx
    mov dword [r12 + r9*4], edx
    inc r15
    jmp .next_k

.check_p2:
    cmp ecx, ebx
    jle .next_k

    align 16
.while_great:
    cmp r9, r8
    jge .swap_great
    mov edx, dword [r12 + r8*4]
    cmp edx, ebx
    jle .swap_great
    dec r8
    jmp .while_great

.swap_great:
    mov edx, dword [r12 + r8*4]
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + r8*4], ecx
    mov dword [r12 + r9*4], edx
    mov ecx, edx
    dec r8

    cmp ecx, eax
    jge .next_k
    mov edx, dword [r12 + r15*4]
    mov dword [r12 + r15*4], ecx
    mov dword [r12 + r9*4], edx
    inc r15
.next_k:
    inc r9
    jmp .while_k

.done_while:
    dec r15
    inc r8

    mov ecx, dword [r12 + r13*4]
    mov edx, dword [r12 + r15*4]
    mov dword [r12 + r13*4], edx
    mov dword [r12 + r15*4], ecx

    mov ecx, dword [r12 + r14*4]
    mov edx, dword [r12 + r8*4]
    mov dword [r12 + r14*4], edx
    mov dword [r12 + r8*4], ecx

    ; If partition did not shrink the outer ranges, use heapsort. The current
    ; stack is already ABI-aligned; do not apply an extra 8-byte adjustment.
    cmp r15, r13
    jne .continue_dp_recursion
    cmp r8, r14
    jne .continue_dp_recursion
    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .ret_dp

.continue_dp_recursion:
    ; Keep r8 in this frame's local slot. r12-r15/rbx are preserved by each
    ; recursive dual_pivot_inner frame according to the SysV ABI.
    mov [rsp], r8
    mov rdi, r12
    mov rsi, r13
    lea rdx, [r15 - 1]
    call dual_pivot_inner
    mov r8, [rsp]

    mov [rsp], r8
    mov rdi, r12
    lea rsi, [r15 + 1]
    lea rdx, [r8 - 1]
    call dual_pivot_inner
    mov r8, [rsp]

    mov rdi, r12
    lea rsi, [r8 + 1]
    mov rdx, r14
    call dual_pivot_inner

.ret_dp:
    add rsp, 16
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.ret_dp_early:
    ret

; ==============================================================================
; MERGE SORT - CMOV BRANCHLESS ОПТИМИЗАЦИЯ
; ==============================================================================
align 16
merge_sort_fasm:
    cmp edx, ecx
    jge .end_merge
    call merge_inner
.end_merge:
    ret

align 16
merge_inner:
    cmp rdx, rcx
    jge .ret_inner

    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    mov rbx, r15
    sub rbx, r14
    shr rbx, 1
    add rbx, r14

    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    mov rcx, rbx
    call merge_inner

    mov rdi, r12
    mov rsi, r13
    mov rdx, rbx
    inc rdx
    mov rcx, r15
    call merge_inner

    mov r8, r14
    mov r9, rbx
    inc r9
    mov r10, r14

    align 16
.merge_loop:
    cmp r8, rbx
    jg .copy_j
    cmp r9, r15
    jg .copy_i

    mov eax, dword [r12 + r8*4]
    mov ecx, dword [r12 + r9*4]

    cmp eax, ecx
    setle dl
    movzx r11, dl
    mov edx, 1
    sub edx, r11d

    cmovle ecx, eax
    mov dword [r13 + r10*4], ecx

    add r8, r11
    add r9, rdx
    inc r10
    jmp .merge_loop

.copy_i:
    cmp r8, rbx
    jg .copy_back
    mov eax, dword [r12 + r8*4]
    mov dword [r13 + r10*4], eax
    inc r8
    inc r10
    jmp .copy_i
.copy_j:
    cmp r9, r15
    jg .copy_back
    mov ecx, dword [r12 + r9*4]
    mov dword [r13 + r10*4], ecx
    inc r9
    inc r10
    jmp .copy_j
.copy_back:
    mov r8, r14
    align 16
.copy_back_loop:
    cmp r8, r15
    jg .end_merge_inner
    mov eax, dword [r13 + r8*4]
    mov dword [r12 + r8*4], eax
    inc r8
    jmp .copy_back_loop
.end_merge_inner:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.ret_inner:
    ret


; ==============================================================================
; HEAP SORT & HEAPIFY
; ==============================================================================
heapify_fasm:
.loop:
    mov r8, rdx
    lea r9, [rdx * 2 + 1]
    lea r10, [rdx * 2 + 2]
    cmp r9, rsi
    jge .check_r
    mov eax, dword [rdi + r9*4]
    cmp eax, dword [rdi + r8*4]
    jle .check_r
    mov r8, r9
.check_r:
    cmp r10, rsi
    jge .check_largest
    mov eax, dword [rdi + r10*4]
    cmp eax, dword [rdi + r8*4]
    jle .check_largest
    mov r8, r10
.check_largest:
    cmp r8, rdx
    je .done
    mov eax, dword [rdi + rdx*4]
    mov ecx, dword [rdi + r8*4]
    mov dword [rdi + rdx*4], ecx
    mov dword [rdi + r8*4], eax
    mov rdx, r8
    jmp .loop
.done:
    ret

heap_sort_fasm:
    cmp esi, 1
    jle .end_heap
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    sub rsp, 8

    mov r12, rdi
    movsxd r13, esi
    mov rax, r13
    shr rax, 1
    dec rax
    mov r14, rax
.build_heap_loop:
    cmp r14, 0
    jl .sort_loop_init
    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    call heapify_fasm
    dec r14
    jmp .build_heap_loop
.sort_loop_init:
    mov r14, r13
    dec r14
.sort_loop:
    cmp r14, 0
    jle .restore_heap
    mov eax, dword [r12]
    mov ecx, dword [r12 + r14*4]
    mov dword [r12], ecx
    mov dword [r12 + r14*4], eax

    mov rdi, r12
    mov rsi, r14
    xor rdx, rdx
    call heapify_fasm
    dec r14
    jmp .sort_loop
.restore_heap:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbp
.end_heap:
    ret


; ==============================================================================
; RADIX SORT
; ==============================================================================
radix_sort_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 1032

    mov r12, rdi
    mov r13, rsi
    movsxd r14, edx
    xor r15, r15
.outer_loop:
    cmp r15, 32
    jge .done_radix

    lea rdi, [rsp]
    xor eax, eax
    mov ecx, 256
    rep stosd

    xor r8, r8
.count_loop:
    cmp r8, r14
    jge .prefix_sums
    mov eax, dword [r12 + r8*4]
    mov cl, r15b
    shr eax, cl
    and eax, 0xFF
    ; Signed-int radix order: invert the most significant byte's sign bit.
    cmp r15, 24
    jne .count_key_ready
    xor eax, 0x80
.count_key_ready:
    inc dword [rsp + rax*4]
    inc r8
    jmp .count_loop
.prefix_sums:
    mov r8, 1
.prefix_loop:
    cmp r8, 256
    jge .build_output
    mov eax, dword [rsp + r8*4 - 4]
    add dword [rsp + r8*4], eax
    inc r8
    jmp .prefix_loop
.build_output:
    mov r8, r14
    dec r8
.build_loop:
    cmp r8, 0
    jl .copy_back
    mov eax, dword [r12 + r8*4]
    mov edx, eax
    mov cl, r15b
    shr eax, cl
    and eax, 0xFF
    cmp r15, 24
    jne .build_key_ready
    xor eax, 0x80
.build_key_ready:
    mov ebx, dword [rsp + rax*4]
    dec ebx
    mov dword [rsp + rax*4], ebx

    movsxd r9, ebx
    mov dword [r13 + r9*4], edx
    dec r8
    jmp .build_loop
.copy_back:
    xor r8, r8
.copy_loop:
    cmp r8, r14
    jge .next_shift
    mov eax, dword [r13 + r8*4]
    mov dword [r12 + r8*4], eax
    inc r8
    jmp .copy_loop
.next_shift:
    add r15, 8
    jmp .outer_loop
.done_radix:
    add rsp, 1032
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; ВНОВЬ ДОБАВЛЕННЫЙ БЛОК: BITONIC SORT (Обертка и рекурсивное построение)
; ==============================================================================
align 16
bitonic_sort_rec_fasm:
    ; rdi = arr, rsi = start, rdx = len, rcx = dir
    cmp rdx, 1
    jle .end_rec

    push r12
    push r13
    push r14
    push r15
    push rbx

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    mov rbx, r14
    shr rbx, 1      ; k = len / 2

    ; возврастание
    mov rdi, r12
    mov rsi, r13
    mov rdx, rbx
    mov rcx, 1
    call bitonic_sort_rec_fasm

    ; убывание
    mov rdi, r12
    mov rsi, r13
    add rsi, rbx
    mov rdx, rbx
    xor rcx, rcx
    call bitonic_sort_rec_fasm

    mov rdi, r12
    mov rsi, r13
    mov rdx, r14
    mov rcx, r15
    call bitonic_merge_fasm

    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.end_rec:
    ret

align 16
bitonic_sort_fasm:
    ; rdi = arr, esi = n
    cmp esi, 1
    jle .end_sort

    sub rsp, 8

    movsxd rdx, esi ; len = n
    xor rsi, rsi    ; start = 0
    mov rcx, 1      ; dir = 1
    call bitonic_sort_rec_fasm

    add rsp, 8
.end_sort:
    ret

; ==============================================================================
; BITONIC MERGE С ИСПОЛЬЗОВАНИЕМ AVX2 ВЕКТОРИЗАЦИИ
; ==============================================================================
align 16
bitonic_merge_fasm:
    cmp rdx, 1
    jle .end_merge
    push r12
    push r13
    push r14
    push r15
    push rbx

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    mov rbx, r14
    shr rbx, 1
    mov r8, r13
    mov r9, r13
    add r9, rbx

    cmp rbx, 8
    jge .avx2_merge_loop

    align 16
.merge_loop:
    cmp r8, r9
    jge .recursive_calls

    mov eax, dword [r12 + r8*4]
    mov r10, r8
    add r10, rbx
    mov edx, dword [r12 + r10*4]

    cmp eax, edx
    setg cl
    movzx rcx, cl
    cmp r15, rcx
    jne .next_iter

    mov dword [r12 + r8*4], edx
    mov dword [r12 + r10*4], eax
.next_iter:
    inc r8
    jmp .merge_loop

    align 16
.avx2_merge_loop:
    cmp r8, r9
    jge .recursive_calls

    mov r10, r8
    add r10, rbx

    vmovdqu ymm0, [r12 + r8*4]
    vmovdqu ymm1, [r12 + r10*4]

    cmp r15, 1
    jne .descending_avx
.ascending_avx:
    vpminud ymm2, ymm0, ymm1
    vpmaxud ymm3, ymm0, ymm1
    jmp .store_avx
.descending_avx:
    vpmaxud ymm2, ymm0, ymm1
    vpminud ymm3, ymm0, ymm1
.store_avx:
    vmovdqu [r12 + r8*4], ymm2
    vmovdqu [r12 + r10*4], ymm3
    add r8, 8
    jmp .avx2_merge_loop

.recursive_calls:
    mov rdi, r12
    mov rsi, r13
    mov rdx, rbx
    mov rcx, r15
    call bitonic_merge_fasm

    mov rdi, r12
    mov rsi, r13
    add rsi, rbx
    mov rdx, rbx
    mov rcx, r15
    call bitonic_merge_fasm

    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.end_merge:
    ret

; ==============================================================================
; INTROSORT
; ==============================================================================
introsort_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    cmp r13, 1
    jle .exit_intro

    mov rax, r13
    bsr rcx, rax
    shl rcx, 1
    mov r14, rcx

    mov rdi, r12
    xor rsi, rsi
    mov rdx, r13
    dec rdx
    mov rcx, r14
    call introsort_loop_fasm

    mov rdi, r12
    mov rsi, r13
    call insertion_sort_fasm
.exit_intro:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

introsort_loop_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    mov rax, r14
    sub rax, r13
    cmp rax, 16
    jl .exit_loop

    cmp r15, 0
    jne .partition

    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .exit_loop
.partition:
    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov ebx, dword [r12 + rax*4]

    mov rax, r13
    dec rax
    mov rdx, r14
    inc rdx
.p_loop:
.i_loop:
    inc rax
    cmp dword [r12 + rax*4], ebx
    jl .i_loop
.j_loop:
    dec rdx
    cmp dword [r12 + rdx*4], ebx
    jg .j_loop

    cmp rax, rdx
    jge .p_done

    mov r8d, dword [r12 + rax*4]
    mov r9d, dword [r12 + rdx*4]
    mov dword [r12 + rax*4], r9d
    mov dword [r12 + rdx*4], r8d
    jmp .p_loop
.p_done:
    mov rdi, r12
    mov rsi, r13
    mov rcx, r15
    dec rcx
    push rdx
    call introsort_loop_fasm
    pop rdx

    mov rdi, r12
    mov rsi, rdx
    inc rsi
    mov rdx, r14
    mov rcx, r15
    dec rcx
    call introsort_loop_fasm
.exit_loop:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; TIMSORT
; ==============================================================================
timsort_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    cmp r14, 1
    jle .end_timsort

    xor r15, r15
.insert_blocks:
    cmp r15, r14
    jge .merge_setup

    mov rax, r15
    add rax, 32
    cmp rax, r14
    jle .set_len
    mov rax, r14
.set_len:
    sub rax, r15

    lea rdi, [r12 + r15*4]
    mov rsi, rax
    call insertion_sort_fasm

    add r15, 32
    jmp .insert_blocks
.merge_setup:
    mov ebx, 32
.merge_outer:
    movsxd rax, ebx
    cmp rax, r14
    jge .end_timsort

    xor r15, r15
.merge_inner:
    cmp r15, r14
    jge .next_size

    mov rax, r15
    add rax, rbx
    dec rax

    mov rcx, r14
    dec rcx
    cmp rax, rcx
    jge .next_size

    mov r8, rbx
    shl r8, 1
    add r8, r15
    dec r8

    cmp r8, r14
    jl .set_right
    mov r8, r14
    dec r8
.set_right:
    push rax
    push r8

    mov r9, r15
    mov r10, rax
    inc r10
    mov r11, r15
.m_loop:
    cmp r9, rax
    jg .m_rem_j
    cmp r10, r8
    jg .m_rem_i

    mov edx, dword [r12 + r9*4]
    mov edi, dword [r12 + r10*4]
    cmp edx, edi
    jg .take_right
    mov dword [r13 + r11*4], edx
    inc r9
    inc r11
    jmp .m_loop
.take_right:
    mov dword [r13 + r11*4], edi
    inc r10
    inc r11
    jmp .m_loop
.m_rem_i:
    cmp r9, rax
    jg .m_copy_back
    mov edx, dword [r12 + r9*4]
    mov dword [r13 + r11*4], edx
    inc r9
    inc r11
    jmp .m_rem_i
.m_rem_j:
    cmp r10, r8
    jg .m_copy_back
    mov edi, dword [r12 + r10*4]
    mov dword [r13 + r11*4], edi
    inc r10
    inc r11
    jmp .m_rem_j
.m_copy_back:
    pop r8
    pop rax
    mov r9, r15
.copy_loop:
    cmp r9, r8
    jg .next_inner
    mov edx, dword [r13 + r9*4]
    mov dword [r12 + r9*4], edx
    inc r9
    jmp .copy_loop
.next_inner:
    mov rax, rbx
    shl rax, 1
    add r15, rax
    jmp .merge_inner
.next_size:
    shl ebx, 1
    jmp .merge_outer
.end_timsort:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; PDQSORT (PATTERN-DEFEATING QUICKSORT)
; ==============================================================================
pdqsort_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    cmp r13, 1
    jle .exit_pdq

    mov rax, r13
    bsr rcx, rax
    shl rcx, 1
    mov r14, rcx

    mov rdi, r12
    xor rsi, rsi
    mov rdx, r13
    dec rdx
    mov rcx, r14
    call pdqsort_loop_fasm

    mov rdi, r12
    mov rsi, r13
    call insertion_sort_fasm
.exit_pdq:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

pdqsort_loop_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    mov r15, rcx

    mov rax, r14
    sub rax, r13
    cmp rax, 16
    jl .exit_loop

    cmp r15, 0
    jne .branchless_partition

    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .exit_loop
.branchless_partition:
    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov ecx, dword [r12 + rax*4]
    mov ebx, dword [r12 + r14*4]
    mov dword [r12 + rax*4], ebx
    mov dword [r12 + r14*4], ecx
    mov r11d, ecx

    mov r8, r13
    dec r8
    mov r9, r13
.p_loop:
    cmp r9, r14
    jge .p_done

    mov eax, dword [r12 + r9*4]
    cmp eax, r11d
    setl cl
    movzx rcx, cl
    lea r8, [r8 + rcx]

    mov r10, r9
    cmovl r10, r8

    mov ebx, dword [r12 + r10*4]
    mov dword [r12 + r10*4], eax
    mov dword [r12 + r9*4], ebx
    inc r9
    jmp .p_loop
.p_done:
    inc r8
    mov eax, dword [r12 + r8*4]
    mov ebx, dword [r12 + r14*4]
    mov dword [r12 + r8*4], ebx
    mov dword [r12 + r14*4], eax

    push r8
    mov rdi, r12
    mov rsi, r13
    mov rdx, r8
    dec rdx
    mov rcx, r15
    dec rcx
    call pdqsort_loop_fasm
    pop r8

    mov rdi, r12
    mov rsi, r8
    inc rsi
    mov rdx, r14
    mov rcx, r15
    dec rcx
    call pdqsort_loop_fasm
.exit_loop:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; QUICKSELECT (Поиск k-й статистики)
; ==============================================================================
quickselect_fasm:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi
    movsxd r13, esi
    movsxd r14, edx
    movsxd r15, ecx

.loop_qs:
    cmp r13, r14
    jge .found

    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov r8d, dword [r12 + rax*4]

    mov rcx, r13
    dec rcx
    mov rdx, r14
    inc rdx

.partition_loop:
.while_i:
    inc rcx
    cmp dword [r12 + rcx*4], r8d
    jl .while_i
.while_j:
    dec rdx
    cmp dword [r12 + rdx*4], r8d
    jg .while_j

    cmp rcx, rdx
    jge .part_done

    mov eax, dword [r12 + rcx*4]
    mov r10d, dword [r12 + rdx*4]
    mov dword [r12 + rcx*4], r10d
    mov dword [r12 + rdx*4], eax
    jmp .partition_loop

.part_done:
    cmp r15, rdx
    jg .go_right
    mov r14, rdx
    jmp .loop_qs
.go_right:
    mov r13, rdx
    inc r13
    jmp .loop_qs

.found:
    movsxd rax, r13d
    mov eax, dword [r12 + rax*4]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; PARTIAL SORT (Частичная сортировка первых k элементов)
; ==============================================================================
partial_sort_fasm:
    cmp edx, 0
    jle .end_part
    cmp esi, 1
    jle .end_part

    push rbp
    mov rbp, rsp
    push r12
    push r13

    mov r12, rdi
    movsxd r13, edx
    movsxd r8, esi

    cmp r13, r8
    jl .do_select
    mov r13, r8

.do_select:
    mov rdi, r12
    xor rsi, rsi
    mov rdx, r8
    dec rdx
    mov rcx, r13
    dec rcx
    call quickselect_fasm

    mov rdi, r12
    mov rsi, r13
    call pdqsort_fasm

    pop r13
    pop r12
    pop rbp
.end_part:
    ret

section '.text' executable align 16

macro cs3_swap r1, r2 {
    local .skip
    cmp r1, r2
    jle .skip
    xchg r1, r2
  .skip:
}

; ------------------------------------------------------------------------------
; Three-pivot CS_Sort
; ABI: void cs_sort_fasm(int *arr [rdi], int low [esi], int high [edx])
; Five positions are sampled and a 9-comparator sorting network orders them.
; The inner three values become Q1/median/Q3-like pivots. The partition has four
; non-overlapping buckets; repeated sample values trigger a 3-way duplicate path.
; Introspective depth limit falls back to heapsort.
; ------------------------------------------------------------------------------
align 16
cs_sort_fasm:
cs3_opt_fasm:
    cmp esi, edx
    jge .cs_public_ret

    mov eax, edx
    sub eax, esi
    inc eax                    ; n = high - low + 1
    bsr ecx, eax               ; floor(log2(n)); n >= 2
    shl ecx, 1                 ; 2 * floor(log2(n))
    jmp cs3_opt_inner_fasm
.cs_public_ret:
    ret

align 16
cs3_opt_inner_fasm:
    cmp esi, edx
    jge .inner_early_ret

    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 72                ; keep RSP 16-byte aligned before calls

    mov [rbp-48], rdi          ; arr
    mov [rbp-56], rsi          ; low
    mov [rbp-64], rdx          ; high
    mov [rbp-112], rcx         ; depth budget

    mov rax, rdx
    sub rax, rsi
    inc rax                    ; n
    cmp eax, 24
    jl .insertion_sort

    cmp qword [rbp-112], 0
    jle .heap_fallback
    dec qword [rbp-112]

    ; Five samples: first, quarter, middle, three-quarter, last.
    ; r12d/r13d/r14d/r15d/ebx hold the samples until the network finishes.
    mov rbx, [rbp-48]
    mov rax, [rbp-56]
    mov rdx, [rbp-64]
    mov rcx, rdx
    sub rcx, rax
    inc rcx                    ; n

    mov r12d, dword [rbx + rax*4]
    mov rdx, rcx
    shr rdx, 2
    add rdx, [rbp-56]
    mov r13d, dword [rbx + rdx*4]
    mov rdx, rcx
    shr rdx, 1
    add rdx, [rbp-56]
    mov r14d, dword [rbx + rdx*4]
    mov rdx, rcx
    shr rdx, 2
    imul rdx, 3
    mov rax, rcx
    and eax, 3
    imul rax, 3
    shr rax, 2
    add rdx, rax
    add rdx, [rbp-56]
    mov r15d, dword [rbx + rdx*4]
    mov rdx, [rbp-64]
    mov ebx, dword [rbx + rdx*4]

    cs3_swap r12d, r13d
    cs3_swap r15d, ebx
    cs3_swap r14d, ebx
    cs3_swap r14d, r15d
    cs3_swap r13d, ebx
    cs3_swap r12d, r15d
    cs3_swap r12d, r14d
    cs3_swap r13d, r15d
    cs3_swap r13d, r14d

    mov dword [rbp-68], r13d    ; p1 = second sample
    mov dword [rbp-72], r14d    ; p2 = median sample
    mov dword [rbp-76], r15d    ; p3 = fourth sample

    ; Any adjacent equal values in the sorted sample suggest low cardinality.
    cmp r12d, r13d
    je .partition_three_way
    cmp r13d, r14d
    je .partition_three_way
    cmp r14d, r15d
    je .partition_three_way
    cmp r15d, ebx
    je .partition_three_way

    ; Four-bucket partition state: lt, b, i, gt.
    mov rax, [rbp-56]
    mov [rbp-80], rax           ; lt
    mov [rbp-88], rax           ; b
    mov [rbp-96], rax           ; i
    mov rax, [rbp-64]
    mov [rbp-104], rax          ; gt

.partition_four_loop:
    mov rax, [rbp-96]
    cmp rax, [rbp-104]
    jg .partition_four_done

    mov rbx, [rbp-48]
    mov r8d, dword [rbx + rax*4] ; current
    cmp r8d, dword [rbp-72]     ; current < p2?
    jge .partition_ge_p2

    cmp r8d, dword [rbp-68]     ; current < p1?
    jl .partition_lt_p1

    ; p1 <= current < p2: move into bucket #2.
    mov r9, [rbp-88]             ; b
    mov ecx, dword [rbx + r9*4]
    mov rax, [rbp-96]            ; i
    mov dword [rbx + rax*4], ecx
    mov dword [rbx + r9*4], r8d
    inc qword [rbp-88]
    inc qword [rbp-96]
    jmp .partition_four_loop

.partition_lt_p1:
    ; current < p1: rotate A[i], A[b], A[lt] into their proper buckets.
    mov r9, [rbp-80]             ; lt
    mov r10, [rbp-88]            ; b
    mov rax, [rbp-96]            ; i
    mov ecx, dword [rbx + r10*4]
    mov edx, dword [rbx + r9*4]
    mov dword [rbx + rax*4], ecx
    mov dword [rbx + r10*4], edx
    mov dword [rbx + r9*4], r8d
    inc qword [rbp-80]
    inc qword [rbp-88]
    inc qword [rbp-96]
    jmp .partition_four_loop

.partition_ge_p2:
    cmp r8d, dword [rbp-76]     ; current < p3?
    jl .partition_mid3

    ; current >= p3: exchange with the unclassified rightmost item.
    mov r9, [rbp-104]            ; gt
    mov ecx, dword [rbx + r9*4]
    mov rax, [rbp-96]            ; i
    mov dword [rbx + rax*4], ecx
    mov dword [rbx + r9*4], r8d
    dec qword [rbp-104]
    jmp .partition_four_loop    ; i is intentionally not advanced

.partition_mid3:
    ; p2 <= current < p3
    inc qword [rbp-96]
    jmp .partition_four_loop

.partition_four_done:
    ; Boundaries are low..lt-1, lt..b-1, b..i-1, i..high.
    ; First bucket.
    mov rdi, [rbp-48]
    mov rsi, [rbp-56]
    mov rdx, [rbp-80]
    dec rdx
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm

    ; Second bucket.
    mov rdi, [rbp-48]
    mov rsi, [rbp-80]
    mov rdx, [rbp-88]
    dec rdx
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm

    ; Third bucket.
    mov rdi, [rbp-48]
    mov rsi, [rbp-88]
    mov rdx, [rbp-96]
    dec rdx
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm

    ; Fourth bucket: starts at i, which equals the final gt+1.
    mov rdi, [rbp-48]
    mov rsi, [rbp-96]
    mov rdx, [rbp-64]
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm
    jmp .exit_frame

.partition_three_way:
    ; A repeated sample triggers duplicate-aware 3-way partitioning.
    mov rax, [rbp-56]
    mov [rbp-80], rax           ; lt
    mov [rbp-96], rax           ; i
    mov rax, [rbp-64]
    mov [rbp-104], rax          ; gt

.partition_three_loop:
    mov rax, [rbp-96]
    cmp rax, [rbp-104]
    jg .partition_three_done
    mov rbx, [rbp-48]
    mov r8d, dword [rbx + rax*4]
    cmp r8d, dword [rbp-72]
    jl .partition_three_less
    jg .partition_three_greater
    inc qword [rbp-96]          ; equal: leave in the middle group
    jmp .partition_three_loop

.partition_three_less:
    mov r9, [rbp-80]             ; lt
    mov ecx, dword [rbx + r9*4]
    mov dword [rbx + r9*4], r8d
    mov dword [rbx + rax*4], ecx
    inc qword [rbp-80]
    inc qword [rbp-96]
    jmp .partition_three_loop

.partition_three_greater:
    mov r9, [rbp-104]            ; gt
    mov ecx, dword [rbx + r9*4]
    mov dword [rbx + r9*4], r8d
    mov dword [rbx + rax*4], ecx
    dec qword [rbp-104]          ; classify the swapped-in item next
    jmp .partition_three_loop

.partition_three_done:
    mov rdi, [rbp-48]
    mov rsi, [rbp-56]
    mov rdx, [rbp-80]
    dec rdx
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm

    mov rdi, [rbp-48]
    mov rsi, [rbp-104]
    inc rsi
    mov rdx, [rbp-64]
    mov rcx, [rbp-112]
    call cs3_opt_inner_fasm
    jmp .exit_frame

.heap_fallback:
    mov rdi, [rbp-48]
    mov rax, [rbp-56]
    lea rdi, [rdi + rax*4]
    mov rsi, [rbp-64]
    sub rsi, [rbp-56]
    inc rsi
    call heap_sort_fasm
    jmp .exit_frame

.insertion_sort:
    mov rbx, [rbp-48]
    mov r12, [rbp-56]
    inc r12                    ; i = low + 1
.insertion_outer:
    cmp r12, [rbp-64]
    jg .exit_frame
    mov r13d, dword [rbx + r12*4] ; key
    mov r14, r12
    dec r14                    ; j = i - 1
.insertion_inner:
    cmp r14, [rbp-56]
    jl .insertion_place
    mov eax, dword [rbx + r14*4]
    cmp eax, r13d
    jle .insertion_place
    lea rcx, [r14 + 1]
    mov dword [rbx + rcx*4], eax
    dec r14
    jmp .insertion_inner
.insertion_place:
    lea rcx, [r14 + 1]
    mov dword [rbx + rcx*4], r13d
    inc r12
    jmp .insertion_outer

.exit_frame:
    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.inner_early_ret:
    ret



; ------------------------------------------------------------------------------
; CS3-GS hybrid: one Artamonov CAlgorithm(value-domain) split, followed by the
; existing three-pivot CS3 sorter for each ordered bucket.
; The C helper only prepares three signed 64-bit thresholds and returns 0 when
; the input is too small/narrow/skewed. The hot four-way partition is native asm.
; ABI: void cs3_gs_sort_fasm(int *arr [rdi], int low [esi], int high [edx])
; ------------------------------------------------------------------------------
align 16
cs3_gs_sort_fasm:
    cmp esi, edx
    jge .cs3gs_return
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 72                 ; preserve 16-byte alignment before calls

    mov r12, rdi                ; array
    mov r13d, esi               ; low
    mov r14d, edx               ; high

    ; int cs3_gs_make_thresholds(arr, low, high, int64_t cuts[3])
    mov rdi, r12
    mov esi, r13d
    mov edx, r14d
    lea rcx, [rbp-64]           ; thresholds[0..2] occupy rbp-64..rbp-48
    call cs3_gs_make_thresholds
    test eax, eax
    jnz .cs3gs_partition_start

    ; The heuristic declined the generated split: use the original CS3 directly.
    mov rdi, r12
    mov esi, r13d
    mov edx, r14d
    call cs_sort_fasm
    jmp .cs3gs_epilogue

.cs3gs_partition_start:
    mov [rbp-72], r13            ; lt
    mov [rbp-80], r13            ; b
    mov [rbp-88], r13            ; i
    mov [rbp-96], r14            ; gt

.cs3gs_partition_loop:
    mov rax, [rbp-88]             ; i
    cmp rax, [rbp-96]             ; i <= gt?
    jg .cs3gs_partition_done
    movsxd r8, dword [r12 + rax*4]
    cmp r8, [rbp-64]              ; value < t1?
    jl .cs3gs_bucket0
    cmp r8, [rbp-56]              ; value < t2?
    jl .cs3gs_bucket1
    cmp r8, [rbp-48]              ; value < t3?
    jl .cs3gs_bucket2

    ; Fourth bucket: [t3, +infinity), classify the swapped-in value next.
    mov r9, [rbp-96]              ; gt
    cmp rax, r9
    je .cs3gs_bucket3_self
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + rax*4], ecx
    mov dword [r12 + r9*4], r8d
.cs3gs_bucket3_self:
    dec qword [rbp-96]
    jmp .cs3gs_partition_loop

.cs3gs_bucket0:
    ; Three writes put value into [low,lt), preserving both middle buckets.
    mov r9, [rbp-80]              ; b
    mov r10, [rbp-72]             ; lt
    ; Avoid redundant stores when two partition frontiers coincide.
    cmp rax, r9
    je .cs3gs_bucket0_skip_i
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + rax*4], ecx
.cs3gs_bucket0_skip_i:
    cmp r9, r10
    je .cs3gs_bucket0_skip_b
    mov r11d, dword [r12 + r10*4]
    mov dword [r12 + r9*4], r11d
.cs3gs_bucket0_skip_b:
    cmp r10, rax
    je .cs3gs_bucket0_skip_lt
    mov dword [r12 + r10*4], r8d
.cs3gs_bucket0_skip_lt:
    inc qword [rbp-72]
    inc qword [rbp-80]
    inc qword [rbp-88]
    jmp .cs3gs_partition_loop

.cs3gs_bucket1:
    ; Two writes put value into [lt,b).
    mov r9, [rbp-80]              ; b
    cmp rax, r9
    je .cs3gs_bucket1_self
    mov ecx, dword [r12 + r9*4]
    mov dword [r12 + rax*4], ecx
    mov dword [r12 + r9*4], r8d
.cs3gs_bucket1_self:
    inc qword [rbp-80]
    inc qword [rbp-88]
    jmp .cs3gs_partition_loop

.cs3gs_bucket2:
    inc qword [rbp-88]            ; [b,i) is the third bucket
    jmp .cs3gs_partition_loop

.cs3gs_partition_done:
    ; Buckets: [low,lt), [lt,b), [b,i), [i,high]. Sort each using CS3's 3 pivots.
    mov rdi, r12
    mov esi, r13d
    mov rdx, [rbp-72]
    dec rdx
    call cs_sort_fasm

    mov rdi, r12
    mov rsi, [rbp-72]
    mov edx, [rbp-80]
    dec edx
    call cs_sort_fasm

    mov rdi, r12
    mov rsi, [rbp-80]
    mov rdx, [rbp-88]
    dec rdx
    call cs_sort_fasm

    mov rdi, r12
    mov rsi, [rbp-88]
    mov edx, r14d
    call cs_sort_fasm

.cs3gs_epilogue:
    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
.cs3gs_return:
    ret


; ============================================================================
; uint64_t CS3 and CS3-GS variants. Values are always loaded/stored as qwords
; and all key comparisons are UNSIGNED (JB/JBE/JA/JAE), unlike the int32 path.
; The int-sized low/high indices preserve the existing public API convention.
; ============================================================================
macro cs3_swap_u64 r1, r2 {
    local .skip_u64
    cmp r1, r2
    jbe .skip_u64
    xchg r1, r2
  .skip_u64:
}

section '.text' executable align 32

; ABI: void cs_sort_u64_fasm(uint64_t *arr [rdi], int low [esi], int high [edx])
align 16
cs_sort_u64_fasm:
    cmp esi, edx
    jge .csu64_public_ret
    mov eax, edx
    sub eax, esi
    inc eax
    bsr ecx, eax
    shl ecx, 1
    jmp cs3_u64_opt_inner_fasm
.csu64_public_ret:
    ret

align 16
cs3_u64_opt_inner_fasm:
    cmp esi, edx
    jge .csu64_inner_early_ret

    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 104               ; qword slots and 16-byte call alignment

    mov [rbp-48], rdi          ; arr
    mov [rbp-56], rsi          ; low
    mov [rbp-64], rdx          ; high
    mov [rbp-128], rcx         ; introsort depth budget

    mov rax, rdx
    sub rax, rsi
    inc rax
    cmp eax, 24
    jl .csu64_insertion_sort
    cmp qword [rbp-128], 0
    jle .csu64_heap_fallback
    dec qword [rbp-128]

    ; Five samples: first, quarter, middle, three-quarter, last.
    mov rbx, [rbp-48]
    mov rax, [rbp-56]
    mov rdx, [rbp-64]
    mov rcx, rdx
    sub rcx, rax
    inc rcx

    mov r12, qword [rbx + rax*8]
    mov rdx, rcx
    shr rdx, 2
    add rdx, [rbp-56]
    mov r13, qword [rbx + rdx*8]
    mov rdx, rcx
    shr rdx, 1
    add rdx, [rbp-56]
    mov r14, qword [rbx + rdx*8]
    mov rdx, rcx
    shr rdx, 2
    imul rdx, 3
    mov rax, rcx
    and eax, 3
    imul rax, 3
    shr rax, 2
    add rdx, rax
    add rdx, [rbp-56]
    mov r15, qword [rbx + rdx*8]
    mov rdx, [rbp-64]
    mov rbx, qword [rbx + rdx*8]

    cs3_swap_u64 r12, r13
    cs3_swap_u64 r15, rbx
    cs3_swap_u64 r14, rbx
    cs3_swap_u64 r14, r15
    cs3_swap_u64 r13, rbx
    cs3_swap_u64 r12, r15
    cs3_swap_u64 r12, r14
    cs3_swap_u64 r13, r15
    cs3_swap_u64 r13, r14

    mov [rbp-72], r13          ; p1
    mov [rbp-80], r14          ; p2
    mov [rbp-88], r15          ; p3

    cmp r12, r13
    je .csu64_partition_three_way
    cmp r13, r14
    je .csu64_partition_three_way
    cmp r14, r15
    je .csu64_partition_three_way
    cmp r15, rbx
    je .csu64_partition_three_way

    mov rax, [rbp-56]
    mov [rbp-96], rax          ; lt
    mov [rbp-104], rax         ; b
    mov [rbp-112], rax         ; i
    mov rax, [rbp-64]
    mov [rbp-120], rax         ; gt

.csu64_partition_four_loop:
    mov rax, [rbp-112]
    cmp rax, [rbp-120]
    jg .csu64_partition_four_done
    mov rbx, [rbp-48]
    mov r8, qword [rbx + rax*8]
    cmp r8, [rbp-80]           ; unsigned current < p2?
    jae .csu64_ge_p2
    cmp r8, [rbp-72]           ; unsigned current < p1?
    jb .csu64_lt_p1

    ; p1 <= value < p2
    mov r9, [rbp-104]
    mov rcx, qword [rbx + r9*8]
    mov rax, [rbp-112]
    mov qword [rbx + rax*8], rcx
    mov qword [rbx + r9*8], r8
    inc qword [rbp-104]
    inc qword [rbp-112]
    jmp .csu64_partition_four_loop

.csu64_lt_p1:
    mov r9, [rbp-96]
    mov r10, [rbp-104]
    mov rax, [rbp-112]
    mov rcx, qword [rbx + r10*8]
    mov rdx, qword [rbx + r9*8]
    mov qword [rbx + rax*8], rcx
    mov qword [rbx + r10*8], rdx
    mov qword [rbx + r9*8], r8
    inc qword [rbp-96]
    inc qword [rbp-104]
    inc qword [rbp-112]
    jmp .csu64_partition_four_loop

.csu64_ge_p2:
    cmp r8, [rbp-88]           ; unsigned current < p3?
    jb .csu64_mid3
    mov r9, [rbp-120]
    mov rax, [rbp-112]
    cmp rax, r9
    je .csu64_ge_p3_self
    mov rcx, qword [rbx + r9*8]
    mov qword [rbx + rax*8], rcx
    mov qword [rbx + r9*8], r8
.csu64_ge_p3_self:
    dec qword [rbp-120]
    jmp .csu64_partition_four_loop

.csu64_mid3:
    inc qword [rbp-112]
    jmp .csu64_partition_four_loop

.csu64_partition_four_done:
    mov rdi, [rbp-48]
    mov rsi, [rbp-56]
    mov rdx, [rbp-96]
    dec rdx
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm

    mov rdi, [rbp-48]
    mov rsi, [rbp-96]
    mov rdx, [rbp-104]
    dec rdx
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm

    mov rdi, [rbp-48]
    mov rsi, [rbp-104]
    mov rdx, [rbp-112]
    dec rdx
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm

    mov rdi, [rbp-48]
    mov rsi, [rbp-112]
    mov rdx, [rbp-64]
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm
    jmp .csu64_exit_frame

.csu64_partition_three_way:
    mov rax, [rbp-56]
    mov [rbp-96], rax          ; lt
    mov [rbp-112], rax         ; i
    mov rax, [rbp-64]
    mov [rbp-120], rax         ; gt

.csu64_partition_three_loop:
    mov rax, [rbp-112]
    cmp rax, [rbp-120]
    jg .csu64_partition_three_done
    mov rbx, [rbp-48]
    mov r8, qword [rbx + rax*8]
    cmp r8, [rbp-80]
    jb .csu64_partition_three_less
    ja .csu64_partition_three_greater
    inc qword [rbp-112]
    jmp .csu64_partition_three_loop

.csu64_partition_three_less:
    mov r9, [rbp-96]
    mov rcx, qword [rbx + r9*8]
    mov qword [rbx + r9*8], r8
    mov qword [rbx + rax*8], rcx
    inc qword [rbp-96]
    inc qword [rbp-112]
    jmp .csu64_partition_three_loop

.csu64_partition_three_greater:
    mov r9, [rbp-120]
    cmp rax, r9
    je .csu64_partition_three_gt_self
    mov rcx, qword [rbx + r9*8]
    mov qword [rbx + r9*8], r8
    mov qword [rbx + rax*8], rcx
.csu64_partition_three_gt_self:
    dec qword [rbp-120]
    jmp .csu64_partition_three_loop

.csu64_partition_three_done:
    mov rdi, [rbp-48]
    mov rsi, [rbp-56]
    mov rdx, [rbp-96]
    dec rdx
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm

    mov rdi, [rbp-48]
    mov rsi, [rbp-120]
    inc rsi
    mov rdx, [rbp-64]
    mov rcx, [rbp-128]
    call cs3_u64_opt_inner_fasm
    jmp .csu64_exit_frame

.csu64_heap_fallback:
    mov rdi, [rbp-48]
    mov rax, [rbp-56]
    lea rdi, [rdi + rax*8]
    mov rsi, [rbp-64]
    sub rsi, [rbp-56]
    inc rsi
    call cs3_heap_sort_u64_c
    jmp .csu64_exit_frame

.csu64_insertion_sort:
    mov rbx, [rbp-48]
    mov r12, [rbp-56]
    inc r12
.csu64_insertion_outer:
    cmp r12, [rbp-64]
    jg .csu64_exit_frame
    mov r13, qword [rbx + r12*8]
    mov r14, r12
    dec r14
.csu64_insertion_inner:
    cmp r14, [rbp-56]
    jl .csu64_insertion_place
    mov rax, qword [rbx + r14*8]
    cmp rax, r13
    jbe .csu64_insertion_place
    lea rcx, [r14 + 1]
    mov qword [rbx + rcx*8], rax
    dec r14
    jmp .csu64_insertion_inner
.csu64_insertion_place:
    lea rcx, [r14 + 1]
    mov qword [rbx + rcx*8], r13
    inc r12
    jmp .csu64_insertion_outer

.csu64_exit_frame:
    add rsp, 104
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.csu64_inner_early_ret:
    ret

; ABI: void cs3_gs_sort_u64_fasm(uint64_t *arr [rdi], int low [esi], int high [edx])
align 16
cs3_gs_sort_u64_fasm:
    cmp esi, edx
    jge .cs3gsu64_return
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 72

    mov r12, rdi
    mov r13d, esi
    mov r14d, edx

    mov rdi, r12
    mov esi, r13d
    mov edx, r14d
    lea rcx, [rbp-64]          ; uint64_t cuts[3] at rbp-64, -56, -48
    call cs3_gs_make_thresholds_u64
    test eax, eax
    jnz .cs3gsu64_partition_start

    mov rdi, r12
    mov esi, r13d
    mov edx, r14d
    call cs_sort_u64_fasm
    jmp .cs3gsu64_epilogue

.cs3gsu64_partition_start:
    mov [rbp-72], r13          ; lt
    mov [rbp-80], r13          ; b
    mov [rbp-88], r13          ; i
    mov [rbp-96], r14          ; gt

.cs3gsu64_partition_loop:
    mov rax, [rbp-88]
    cmp rax, [rbp-96]
    jg .cs3gsu64_partition_done
    mov r8, qword [r12 + rax*8]
    cmp r8, [rbp-64]
    jb .cs3gsu64_bucket0
    cmp r8, [rbp-56]
    jb .cs3gsu64_bucket1
    cmp r8, [rbp-48]
    jb .cs3gsu64_bucket2

    mov r9, [rbp-96]
    cmp rax, r9
    je .cs3gsu64_bucket3_self
    mov rcx, qword [r12 + r9*8]
    mov qword [r12 + rax*8], rcx
    mov qword [r12 + r9*8], r8
.cs3gsu64_bucket3_self:
    dec qword [rbp-96]
    jmp .cs3gsu64_partition_loop

.cs3gsu64_bucket0:
    mov r9, [rbp-80]
    mov r10, [rbp-72]
    cmp rax, r9
    je .cs3gsu64_bucket0_skip_i
    mov rcx, qword [r12 + r9*8]
    mov qword [r12 + rax*8], rcx
.cs3gsu64_bucket0_skip_i:
    cmp r9, r10
    je .cs3gsu64_bucket0_skip_b
    mov r11, qword [r12 + r10*8]
    mov qword [r12 + r9*8], r11
.cs3gsu64_bucket0_skip_b:
    cmp r10, rax
    je .cs3gsu64_bucket0_skip_lt
    mov qword [r12 + r10*8], r8
.cs3gsu64_bucket0_skip_lt:
    inc qword [rbp-72]
    inc qword [rbp-80]
    inc qword [rbp-88]
    jmp .cs3gsu64_partition_loop

.cs3gsu64_bucket1:
    mov r9, [rbp-80]
    cmp rax, r9
    je .cs3gsu64_bucket1_self
    mov rcx, qword [r12 + r9*8]
    mov qword [r12 + rax*8], rcx
    mov qword [r12 + r9*8], r8
.cs3gsu64_bucket1_self:
    inc qword [rbp-80]
    inc qword [rbp-88]
    jmp .cs3gsu64_partition_loop

.cs3gsu64_bucket2:
    inc qword [rbp-88]
    jmp .cs3gsu64_partition_loop

.cs3gsu64_partition_done:
    mov rdi, r12
    mov esi, r13d
    mov rdx, [rbp-72]
    dec rdx
    call cs_sort_u64_fasm

    mov rdi, r12
    mov rsi, [rbp-72]
    mov rdx, [rbp-80]
    dec rdx
    call cs_sort_u64_fasm

    mov rdi, r12
    mov rsi, [rbp-80]
    mov rdx, [rbp-88]
    dec rdx
    call cs_sort_u64_fasm

    mov rdi, r12
    mov rsi, [rbp-88]
    mov edx, r14d
    call cs_sort_u64_fasm

.cs3gsu64_epilogue:
    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
.cs3gsu64_return:
    ret


; This ELF object does not require an executable stack.
section '.note.GNU-stack'
