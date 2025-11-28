    ;;  Prints ROM fonts address as returned by int 0x10
    use16    86


    .text                   ; text section declaration
    .align 2
    entry start

strlen:
    push di
    push cx
    mov al,#0
    mov cx,#-1
    cmp cx,cx
    cld
    repnz
    scasb
    neg cx
    dec cx
    dec cx
    xchg ax,cx
    pop cx
    pop di
    ret

prints:                         ;  prints string in di
    push ax
    push bx
    push cx
    push dx
    push bp
    push es

    call strlen
    mov cx,di
    mov dx,ax
    mov bx,#1                    ; printing to stdout
    mov ax,#4                    ; write
    int 0x80

    pop es
    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    ret

    ; Prints AX in hex.
printhex:
    push cx

    push ax
    mov al,ah
    mov cl, #4
    shr al, cl
    call print_nibble
    pop ax

    push ax
    mov al,ah
    and al, #0x0F
    call print_nibble
    pop ax

    push ax
    mov cl, #4
    shr al, cl
    call print_nibble
    pop ax

    push ax
    and al, #0x0F
    call print_nibble
    pop ax

    pop cx
    ret

; Prints AL in hex.
printhexb:
    push ax
    push cx
    mov cl, #4
    shr al, cl
    call print_nibble
    pop cx
    pop ax
    and al, #0x0F
    call print_nibble
    ret
print_nibble:
    cmp al, #0x09
    jg .letter
    add al, #0x30
    mov char_ptr, al
    mov cx,#char_ptr
    mov dx,#1
    mov bx,#1                   ; printing to stdout
    mov ax,#4                   ; write
    int 0x80
    ret
.letter:
    add al, #0x37
    mov char_ptr, al
    mov cx,#char_ptr
    mov dx,#1
    mov bx,#1                    ; printing to stdout
    mov ax,#4                    ; write
    int 0x80
    ret


start:
    push    bp
    mov     bp, sp
    sub     sp, #4

    mov di, #msg_banner
    call prints

    mov di, #msg_header
    call prints

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0200                 ; get addr of ROM 8x14 font
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0300                 ; get addr of ROM 8x8 font
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0400                 ; get addr of ROM 8x8 font (2nd half)
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0500                 ; get addr of ROM 9x14 alternate font
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0600                 ; get addr of ROM 8x16 font (VGA)
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0700                 ; get addr of ROM 8x16 alternate font (VGA)
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

    mov     dx,#0
    mov     cx,#0
    mov     ax,#0x1130                 ; function to address the character tables
    mov     bx,#0x0800                 ; wrong function (to see what happens)
    push bp
    push es
    int     0x10
    call print_info
    pop es
    pop bp

exit:
    mov     bx,#0
    mov     ax,#1
    int     0x80

print_info:
    push    dx
    push    cx
    push    bp

    mov     ax,es
    call    printhex
    call    printspc

    pop bp

    mov     ax,bp
    call    printhex
    call    printspc

    pop cx

    mov     ax,cx
    call    printhex
    call    printspc

    pop     dx
    mov     ax,dx
    call    printhex
    call    printnl
    ret

printnl:
    push    ax
    push    bx
    push    cx
    push    dx

    mov     dx, #1
    mov     cx, #new_line
    mov     bx,#1
    mov     ax,#4
    int     0x80

    pop     dx
    pop     cx
    pop     bx
    pop     ax
    ret

printspc:
    push    ax
    push    bx
    push    cx
    push    dx

    mov     dx, #1
    mov     cx, #new_space
    mov     bx,#1
    mov     ax,#4
    int     0x80

    pop     dx
    pop     cx
    pop     bx
    pop     ax
    ret

.data                   ;data section declaration

tempbuffer:
    dw 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    dw 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    dw 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    dw 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

char_ptr:   db 0 ,0
new_line:	db 10, 0
new_space:  db 32, 0

msg_banner:	.ascii "ROM Fonts Checker by Rafael Diniz"
            db   10,0
msg_header: .ascii " ES   BP   CX   DX"
    db 10,0
