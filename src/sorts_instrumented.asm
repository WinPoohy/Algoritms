format ELF64

public cs3_opt_fasm_instr
public dual_pivot_quicksort_fasm_instr
public introsort_fasm_instr
public pdqsort_fasm_instr
public triple_pivot_quicksort_fasm_instr
public cs3_gs_sort_fasm_instr
public cs_sort_u64_fasm_instr
public cs3_gs_sort_u64_fasm_instr

extrn g_cmp_instr
extrn g_mov_instr
extrn heap_sort_fasm
extrn dual_pivot_fallback_heap_instr
extrn cs3_gs_sort_c_instr
extrn cs_sort_u64_c_instr
extrn cs3_gs_sort_u64_c_instr

section '.text' executable align 32

; ==============================================================================
; 1. CS3_OPT_FASM_INSTR
; ==============================================================================
align 16
cs3_opt_fasm_instr:
    cmp esi, edx
    jge .end_cs_instr
    push r12
    push r13
    push r14
    push r15
    push rbx
    push rbp
    sub rsp, 40

    mov r12, rdi        ; arr
    mov r13d, esi       ; low
    mov r14d, edx       ; high

    movsxd r8, r13d
    mov r9d, dword [r12 + r8*4] ; min_val
    mov r10d, r9d               ; max_val
    inc qword [g_mov_instr]
    inc qword [g_mov_instr]
    inc r8

.minmax_loop_instr:
    cmp r8, r14
    jg .minmax_done_instr
    inc qword [g_cmp_instr]
    mov eax, dword [r12 + r8*4]
    cmp eax, r9d
    cmovl r9d, eax
    cmp eax, r10d
    cmovg r10d, eax
    inc r8
    jmp .minmax_loop_instr

.minmax_done_instr:
    mov rdi, r12
    movsxd rsi, r13d
    movsxd rdx, r14d
    call cs3_inner_fasm_instr

    add rsp, 40
    pop rbp
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.end_cs_instr:
    ret

align 16
cs3_inner_fasm_instr:
    cmp rsi, rdx
    jge .ret_cs_in
    push r12
    push r13
    push r14
    push r15
    push rbx

    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov r15d, dword [r12 + rax*4]
    inc qword [g_mov_instr]

    mov r8, r13
    dec r8
    mov r9, r14
    inc r9

.part_loop_instr:
.while_i_instr:
    inc r8
    inc qword [g_cmp_instr]
    mov eax, dword [r12 + r8*4]
    cmp eax, r15d
    jl .while_i_instr

.while_j_instr:
    dec r9
    inc qword [g_cmp_instr]
    mov eax, dword [r12 + r9*4]
    cmp eax, r15d
    jg .while_j_instr

    cmp r8, r9
    jge .part_done_instr

    mov eax, dword [r12 + r8*4]
    mov ebx, dword [r12 + r9*4]
    mov dword [r12 + r8*4], ebx
    mov dword [r12 + r9*4], eax
    add qword [g_mov_instr], 2
    jmp .part_loop_instr

.part_done_instr:
    push r9
    mov rdi, r12
    mov rsi, r13
    mov rdx, r9
    call cs3_inner_fasm_instr

    pop r9
    mov rdi, r12
    mov rsi, r9
    inc rsi
    mov rdx, r14
    call cs3_inner_fasm_instr

    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.ret_cs_in:
    ret

; ==============================================================================
; 2. DUAL_PIVOT_QUICKSORT_FASM_INSTR
; ==============================================================================
align 16
dual_pivot_quicksort_fasm_instr:
    cmp esi, edx
    jge .end_dp_instr
    push r12
    push r13
    push r14
    push r15
    push rbx
    call dp_inner_instr
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
.end_dp_instr:
    ret

align 16
dp_inner_instr:
    cmp rsi, rdx
    jge .ret_dp_in
    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    inc qword [g_cmp_instr]
    mov eax, dword [r12 + r13*4]
    mov ebx, dword [r12 + r14*4]
    cmp eax, ebx
    jle .no_swap_dp
    mov dword [r12 + r13*4], ebx
    mov dword [r12 + r14*4], eax
    xchg eax, ebx
    add qword [g_mov_instr], 3
.no_swap_dp:
    lea r15, [r13 + 1]
    lea r8, [r14 - 1]
    mov r9, r15

.dp_while_k:
    cmp r9, r8
    jg .dp_done_while

    inc qword [g_cmp_instr]
    mov ecx, dword [r12 + r9*4]
    cmp ecx, eax
    jl .dp_less_p1

    inc qword [g_cmp_instr]
    cmp ecx, ebx
    jle .dp_next

.dp_great_loop:
    cmp r9, r8
    jge .dp_swap_gr
    inc qword [g_cmp_instr]
    mov edx, dword [r12 + r8*4]
    cmp edx, ebx
    jle .dp_swap_gr
    dec r8
    jmp .dp_great_loop

.dp_swap_gr:
    mov edx, dword [r12 + r8*4]
    mov dword [r12 + r8*4], ecx
    mov dword [r12 + r9*4], edx
    add qword [g_mov_instr], 2
    dec r8
    inc qword [g_cmp_instr]
    cmp ecx, eax
    jge .dp_next
    mov edx, dword [r12 + r15*4]
    mov dword [r12 + r15*4], ecx
    mov dword [r12 + r9*4], edx
    add qword [g_mov_instr], 2
    inc r15
    jmp .dp_next

.dp_less_p1:
    mov edx, dword [r12 + r15*4]
    mov dword [r12 + r15*4], ecx
    mov dword [r12 + r9*4], edx
    add qword [g_mov_instr], 2
    inc r15

.dp_next:
    inc r9
    jmp .dp_while_k

.dp_done_while:
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
    add qword [g_mov_instr], 6

    ; На отсортированных/однотипных данных dual-pivot может не уменьшать
    ; диапазон. В этом случае вызываем инструментированный heapsort из C.
    cmp r15, r13
    jne .continue_dp_instr_recursion
    cmp r8, r14
    jne .continue_dp_instr_recursion
    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    sub rsp, 8
    call dual_pivot_fallback_heap_instr
    add rsp, 8
    jmp .ret_dp_in

.continue_dp_instr_recursion:
    push r8
    push r14
    push r15

    mov rsi, r13
    lea rdx, [r15 - 1]
    call dp_inner_instr

    pop r15
    pop r14
    pop r8

    push r8
    push r14

    lea rsi, [r15 + 1]
    lea rdx, [r8 - 1]
    call dp_inner_instr

    pop r14
    pop r8

    lea rsi, [r8 + 1]
    mov rdx, r14
    call dp_inner_instr
.ret_dp_in:
    ret

; ==============================================================================
; 3. INTROSORT_FASM_INSTR
; ==============================================================================
align 16
introsort_fasm_instr:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    inc qword [g_cmp_instr]
    cmp r13, 1
    jle .exit_intro_instr

    mov rax, r13
    bsr rcx, rax
    shl rcx, 1
    mov r14, rcx

    mov rdi, r12
    xor rsi, rsi
    mov rdx, r13
    dec rdx
    mov rcx, r14
    call introsort_loop_fasm_instr

.exit_intro_instr:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

align 16
introsort_loop_fasm_instr:
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
    inc qword [g_cmp_instr]
    cmp rax, 16
    jl .exit_loop_in

    inc qword [g_cmp_instr]
    cmp r15, 0
    jne .part_in

    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .exit_loop_in

.part_in:
    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov ebx, dword [r12 + rax*4]
    inc qword [g_mov_instr]

    mov rax, r13
    dec rax
    mov rdx, r14
    inc rdx
.pi_loop:
.pi_i:
    inc rax
    inc qword [g_cmp_instr]
    cmp dword [r12 + rax*4], ebx
    jl .pi_i
.pi_j:
    dec rdx
    inc qword [g_cmp_instr]
    cmp dword [r12 + rdx*4], ebx
    jg .pi_j

    inc qword [g_cmp_instr]
    cmp rax, rdx
    jge .pi_done

    mov r8d, dword [r12 + rax*4]
    mov r9d, dword [r12 + rdx*4]
    mov dword [r12 + rax*4], r9d
    mov dword [r12 + rdx*4], r8d
    add qword [g_mov_instr], 2
    jmp .pi_loop

.pi_done:
    mov rdi, r12
    mov rsi, r13
    mov rcx, r15
    dec rcx
    push rdx
    call introsort_loop_fasm_instr
    pop rdx

    mov rdi, r12
    mov rsi, rdx
    inc rsi
    mov rdx, r14
    mov rcx, r15
    dec rcx
    call introsort_loop_fasm_instr

.exit_loop_in:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; 4. PDQSORT_FASM_INSTR
; ==============================================================================
align 16
pdqsort_fasm_instr:
    push rbp
    mov rbp, rsp
    push r12
    push r13
    push r14
    sub rsp, 8

    mov r12, rdi
    mov r13, rsi
    inc qword [g_cmp_instr]
    cmp r13, 1
    jle .exit_pdq_instr

    mov rax, r13
    bsr rcx, rax
    shl rcx, 1
    mov r14, rcx

    mov rdi, r12
    xor rsi, rsi
    mov rdx, r13
    dec rdx
    mov rcx, r14
    call pdqsort_loop_fasm_instr

.exit_pdq_instr:
    add rsp, 8
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

align 16
pdqsort_loop_fasm_instr:
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
    inc qword [g_cmp_instr]
    cmp rax, 16
    jl .exit_pdq_loop

    inc qword [g_cmp_instr]
    cmp r15, 0
    jne .pdq_branchless

    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call heap_sort_fasm
    jmp .exit_pdq_loop

.pdq_branchless:
    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov ecx, dword [r12 + rax*4]
    mov ebx, dword [r12 + r14*4]
    mov dword [r12 + rax*4], ebx
    mov dword [r12 + r14*4], ecx
    mov r11d, ecx
    add qword [g_mov_instr], 3

    mov r8, r13
    dec r8
    mov r9, r13

.pdq_p_loop:
    inc qword [g_cmp_instr]
    cmp r9, r14
    jge .pdq_p_done

    mov eax, dword [r12 + r9*4]
    inc qword [g_cmp_instr]
    cmp eax, r11d
    setl cl
    movzx rcx, cl
    lea r10, [r8 + rcx]
    cmovl r8, r10

    mov r10, r9
    cmovl r10, r8

    mov ebx, dword [r12 + r10*4]
    mov dword [r12 + r10*4], eax
    mov dword [r12 + r9*4], ebx
    add qword [g_mov_instr], 2
    inc r9
    jmp .pdq_p_loop

.pdq_p_done:
    ; Place pivot at its final index before recursing (same partition as the
    ; production PDQ implementation). The old twin skipped this swap and thus
    ; failed qsort validation on larger/adversarial inputs.
    inc r8
    mov eax, dword [r12 + r8*4]
    mov ebx, dword [r12 + r14*4]
    mov dword [r12 + r8*4], ebx
    mov dword [r12 + r14*4], eax
    add qword [g_mov_instr], 2

    push r8
    mov rdi, r12
    mov rsi, r13
    mov rdx, r8
    dec rdx
    mov rcx, r15
    dec rcx
    call pdqsort_loop_fasm_instr
    pop r8

    mov rdi, r12
    mov rsi, r8
    inc rsi
    mov rdx, r14
    mov rcx, r15
    dec rcx
    call pdqsort_loop_fasm_instr

.exit_pdq_loop:
    add rsp, 8
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==============================================================================
; 5. TRIPLE_PIVOT_QUICKSORT_FASM_INSTR (Обособленная версия со счетчиками)
; ==============================================================================
align 16
triple_pivot_quicksort_fasm_instr:
    cmp rsi, rdx
    jge .done

    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    sub rsp, 56

    mov r12, rdi       ; r12 = arr
    mov r13, rsi       ; r13 = low
    mov r14, rdx       ; r14 = high

    mov rax, r14
    sub rax, r13
    cmp rax, 15
    jg .partition_init

    ; --- СОРТИРОВКА ВСТАВКАМИ (для малых массивов) ---
    mov r8, r13
    inc r8
.ins_outer_loop:
    inc qword [g_cmp_instr]
    cmp r8, r14
    jg .epilog_restore

    movsxd rcx, dword [r12 + r8*4]
    inc qword [g_mov_instr]
    mov r9, r8
    dec r9

.ins_inner_loop:
    inc qword [g_cmp_instr]
    cmp r9, r13
    jl .ins_insert_pos

    movsxd r10, dword [r12 + r9*4]
    inc qword [g_mov_instr]

    inc qword [g_cmp_instr]
    cmp r10, rcx
    jle .ins_insert_pos

    mov dword [r12 + r9*4 + 4], r10d
    inc qword [g_mov_instr]
    dec r9
    jmp .ins_inner_loop

.ins_insert_pos:
    mov dword [r12 + r9*4 + 4], ecx
    inc qword [g_mov_instr]
    inc r8
    jmp .ins_outer_loop

.partition_init:
    ; --- ТРОЙНОЕ РАЗБИЕНИЕ (TRIPLE-PIVOT PARTITIONING) ---
    mov rax, r13
    mov rbx, r14

    movsxd r8, dword [r12 + r13*4]
    movsxd r9, dword [r12 + r14*4]
    inc qword [g_cmp_instr]
    cmp r8, r9
    jle .p_sorted_ends
    mov dword [r12 + r13*4], r9d
    mov dword [r12 + r14*4], r8d
    add qword [g_mov_instr], 2
.p_sorted_ends:

    mov rax, r13
    sub rbx, r13
    mov rcx, 3
    xor rdx, rdx
    div rcx
    add rax, r13
    mov r15, rax        ; r15 = mid1

    mov rax, r14
    sub rax, r13
    mov rcx, 3
    xor rdx, rdx
    div rcx
    neg rax
    add rax, r14
    mov rbp, rax        ; rbp = mid2

    movsxd r8, dword [r12 + r15*4]
    movsxd r9, dword [r12 + rbp*4]
    inc qword [g_cmp_instr]
    cmp r8, r9
    jle .pivots_ready
    mov dword [r12 + r15*4], r9d
    mov dword [r12 + rbp*4], r8d
    add qword [g_mov_instr], 2

.pivots_ready:
    movsxd rsi, dword [r12 + r15*4] ; P1
    movsxd rdi, dword [r12 + rbp*4] ; P2
    inc qword [g_mov_instr]
    inc qword [g_mov_instr]
    inc qword [g_cmp_instr]
    cmp rsi, rdi
    je .tp_fallback_heap_instr

    mov r8, r13
    inc r8              ; l (r8)
    mov r9, r14
    dec r9              ; g (r9)
    mov r10, r8         ; k (r10)

    movsxd rax, dword [r12 + r8*4]
    movsxd rbx, dword [r12 + r15*4]
    mov dword [r12 + r8*4], ebx
    mov dword [r12 + r15*4], eax
    add qword [g_mov_instr], 2

    movsxd rax, dword [r12 + r9*4]
    movsxd rbx, dword [r12 + rbp*4]
    mov dword [r12 + r9*4], ebx
    mov dword [r12 + rbp*4], eax
    add qword [g_mov_instr], 2

    inc r8
    dec r9
    mov r10, r8

.partition_loop:
    inc qword [g_cmp_instr]
    cmp r10, r9
    jg .partition_finish

    movsxd rbx, dword [r12 + r10*4]
    inc qword [g_mov_instr]

    inc qword [g_cmp_instr]
    cmp rbx, rsi
    jl .case_less_than_p1

    inc qword [g_cmp_instr]
    cmp rbx, rdi
    jg .case_greater_than_p2

    inc r10
    jmp .partition_loop

.case_less_than_p1:
    movsxd rax, dword [r12 + r8*4]
    mov dword [r12 + r8*4], ebx
    mov dword [r12 + r10*4], eax
    add qword [g_mov_instr], 2
    inc r8
    inc r10
    jmp .partition_loop

.case_greater_than_p2:
.find_smaller:
    inc qword [g_cmp_instr]
    cmp r10, r9
    jge .partition_loop_dec_g

    movsxd rax, dword [r12 + r9*4]
    inc qword [g_mov_instr]

    inc qword [g_cmp_instr]
    cmp rax, rdi
    jl .found_smaller_element
    jg .dec_g_continue

    dec r9
    jmp .find_smaller

.dec_g_continue:
    dec r9
    jmp .find_smaller

.found_smaller_element:
    inc qword [g_cmp_instr]
    cmp rax, rsi
    jl .swap_three_way

    mov dword [r12 + r9*4], ebx
    mov dword [r12 + r10*4], eax
    add qword [g_mov_instr], 2
    dec r9
    inc r10
    jmp .partition_loop

.swap_three_way:
    movsxd rcx, dword [r12 + r8*4]
    mov dword [r12 + r8*4], eax
    mov dword [r12 + r9*4], ebx
    mov dword [r12 + r10*4], ecx
    add qword [g_mov_instr], 3
    inc r8
    dec r9
    inc r10
    jmp .partition_loop

.partition_loop_dec_g:
    dec r9
    jmp .partition_loop

.partition_finish:
    dec r8
    movsxd rax, dword [r12 + r13*4]
    movsxd rbx, dword [r12 + r8*4]
    mov dword [r12 + r13*4], ebx
    mov dword [r12 + r8*4], eax
    add qword [g_mov_instr], 2

    inc r9
    movsxd rax, dword [r12 + r14*4]
    movsxd rbx, dword [r12 + r9*4]
    mov dword [r12 + r14*4], ebx
    mov dword [r12 + r9*4], eax
    add qword [g_mov_instr], 2

    mov r15, r8
    mov rbp, r9

    mov rdi, r12
    mov rsi, r13
    mov rdx, r15
    sub rdx, 1
    call triple_pivot_quicksort_fasm_instr

    mov rdi, r12
    mov rsi, r15
    add rsi, 1
    mov rdx, rbp
    sub rdx, 1
    call triple_pivot_quicksort_fasm_instr

    mov rdi, r12
    mov rsi, rbp
    add rsi, 1
    mov rdx, r14
    call triple_pivot_quicksort_fasm_instr
    jmp .epilog_restore

.tp_fallback_heap_instr:
    lea rdi, [r12 + r13*4]
    mov rsi, r14
    sub rsi, r13
    inc rsi
    call dual_pivot_fallback_heap_instr
    jmp .epilog_restore

.epilog_restore:
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

.done:
    ret



; CS3-GS's instrumented twin intentionally reuses the matching C instrumentation
; so every operation (including the generating-sequence scan/threshold split and
; the three-pivot leaf sorter) uses the same documented counting convention.
align 16
cs3_gs_sort_fasm_instr:
    jmp cs3_gs_sort_c_instr


; Full-width uint64_t CS3 instrumented entry point. We delegate the operation
; counting to the matching C twin so the C and FASM labels share one counting
; convention; this routine is intended for complexity counts, not timings.
align 16
cs_sort_u64_fasm_instr:
    jmp cs_sort_u64_c_instr

; Full-width uint64_t CS3-GS instrumented entry point, using the same counters.
align 16
cs3_gs_sort_u64_fasm_instr:
    jmp cs3_gs_sort_u64_c_instr

; This ELF object does not require an executable stack.
section '.note.GNU-stack'
