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
public cs_dual_partition

extrn pthread_create
extrn pthread_join

section '.text' executable align 32

; ==============================================================================
; TRIPLE-PIVOT QUICKSORT (Разделение на 4 части)
; ==============================================================================
align 16
triple_pivot_quicksort_fasm:
    cmp esi, edx
    jge .end_tp
    jmp triple_pivot_inner
.end_tp:
    ret

align 16
triple_pivot_inner:
    push r12
    push r13
    push r14
    push r15
    push rbx
    sub rsp, 64

    mov r12, rdi        ; arr
    mov r13, rsi        ; low
    mov r14, rdx        ; high

    mov [rsp+24], r13
    mov [rsp+32], r14

    mov rax, r14
    sub rax, r13
    cmp rax, 2
    jge .tp_partition

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
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+40]
    dec rsi
    mov rdx, [rsp+48]
    sub rdx, 2
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+48]
    mov rdx, [rsp+56]
    call triple_pivot_inner

    mov rdi, r12
    mov rsi, [rsp+56]
    add rsi, 2
    mov rdx, [rsp+32]
    call triple_pivot_inner

.exit_tp:
    add rsp, 64
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
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
; QUICKSORT (HOARE PARTITION)
; ==============================================================================
align 16
quicksort_fasm:
    cmp esi, edx
    jge .end_qs
    call quicksort_inner
.end_qs:
    ret

align 16
quicksort_inner:
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

    mov rax, r14
    sub rax, r13
    shr rax, 1
    add rax, r13
    mov r15d, dword [r12 + rax*4] ; pivot

    mov r8, r13
    dec r8
    mov r9, r14
    inc r9
    align 16
.partition_loop:
    align 16
.while_i:
    inc r8
    cmp dword [r12 + r8*4], r15d
    jl .while_i
    align 16
.while_j:
    dec r9
    cmp dword [r12 + r9*4], r15d
    jg .while_j

    cmp r8, r9
    jge .partition_done

    mov eax, dword [r12 + r8*4]
    mov r10d, dword [r12 + r9*4]
    mov dword [r12 + r8*4], r10d
    mov dword [r12 + r9*4], eax
    jmp .partition_loop
.partition_done:
    mov rdi, r12
    mov rsi, r13
    mov rdx, r9
    call quicksort_inner

    mov rdi, r12
    mov rsi, r9
    inc rsi
    mov rdx, r14
    call quicksort_inner

    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
.ret_inner:
    ret

; ==============================================================================
; QUICKSORT BRANCHLESS (LOMUTO + ALIGNMENT)
; ==============================================================================
align 16
quicksort_branchless_fasm:
    cmp esi, edx
    jge .end_qsb
    call quicksort_branchless_inner
.end_qsb:
    ret

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

    mov rdi, r12
    lea rsi, [r14 + 1]
    lea rdx, [r13 - 1]
    call quicksort_fasm

    mov rdi, [rsp]
    xor rsi, rsi
    call pthread_join

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
    jge .ret_dp

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

    push r8
    push r14
    push r15

    mov rsi, r13
    lea rdx, [r15 - 1]
    call dual_pivot_inner

    pop r15
    pop r14
    pop r8

    push r8
    push r14

    lea rsi, [r15 + 1]
    lea rdx, [r8 - 1]
    call dual_pivot_inner

    pop r14
    pop r8

    lea rsi, [r8 + 1]
    mov rdx, r14
    call dual_pivot_inner
.ret_dp:
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

; void cs_dual_partition(int* rdi [arr], int esi [len], int edx [p1], int ecx [p2], int* r8 [out_i], int* r9 [out_j])
cs_dual_partition:
    test esi, esi
    jle .done_empty

    push r12
    push r13

    xor r10d, r10d          ; i = 0
    lea r11d, [rsi - 1]     ; j = len - 1
    xor eax, eax            ; k = 0

    align 16
.loop_start:
    cmp eax, r11d
    jg .done

    ; чтение arr[k] в регистр r12d
    movsxd rax, eax
    mov r12d, dword [rdi + rax*4]

    ; проверка условия: arr[k] < p1
    cmp r12d, edx
    jl .less_p1

    ; проверка условия: arr[k] > p2
    cmp r12d, ecx
    jg .greater_p2

    ; если p1 <= arr[k] <= p2
    inc eax
    jmp .loop_start

    align 16
.less_p1:
    ; swap(arr[k], arr[i])
    movsxd r10, r10d
    mov r13d, dword [rdi + r10*4]
    mov dword [rdi + rax*4], r13d
    mov dword [rdi + r10*4], r12d
    inc r10d                ; i++
    inc eax                 ; k++
    jmp .loop_start

    align 16
.greater_p2:
    ; swap(arr[k], arr[j])
    movsxd r11, r11d
    mov r13d, dword [rdi + r11*4]
    mov dword [rdi + rax*4], r13d
    mov dword [rdi + r11*4], r12d
    dec r11d                ; j--
    jmp .loop_start

.done:
    mov dword [r8], r10d
    mov dword [r9], r11d
    pop r13
    pop r12
    ret

.done_empty:
    mov dword [r8], 0
    mov dword [r9], 0
    ret
