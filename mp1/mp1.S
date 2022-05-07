# ECE 391 MP1:
# PEIYUAN LIU

.data					# section declaration

        # Useful offset constants for accessing members of a 
        # struct mp1_blink_struct structure
        LOCATION   = 0    
        ON_CHAR    = 2
        OFF_CHAR   = 3 
        ON_LENGTH  = 4
        OFF_LENGTH = 6
        COUNTDOWN  = 8
        STATUS     = 10
        NEXT       = 12

        STRUCT_SIZE = 16

# Pointer to head of list (initialized to NULL)
mp1_list_head:
        .long   0

.text					# section declaration

# Export the function symbol names

.global mp1_rtc_tasklet
.global mp1_ioctl

# void mp1_poke(void);
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX
mp1_poke:
	movl    vmem_base_addr(,1),%edx
	movb    %cl,(%edx,%eax,1)
	ret

# mp1_rtc_tasklet
#
# Description: Go through the mp1_list_head list, examine each struct,
# update the value of struct
# Register: %eax - 1. move data (temporary usage)
#                  2. store the offset of mp1_poke
#           %ecx - 1. store the status
#                  2. store the display char of mp1_poke
#           %edx - 1. the pointer of the current struct
# Output: None
mp1_rtc_tasklet:
        pushl   %ebp                    # set up stack frame
        movl    %esp, %ebp

        movl    mp1_list_head, %edx     # edx <-- mp1_list_head

search_begin:
        cmpl    $0, %edx                # edx - 0
        je      search_done

        decw    COUNTDOWN(%edx)         # countdown = countdown - 1
        cmpw    $0, COUNTDOWN(%edx)     # countdown - 0
        jne     search_next

        xorl    %eax, %eax              
        movw    STATUS(%edx), %ax                       
        movb    ON_CHAR(%edx, %eax), %cl         # cl <-- on_char/offchar
        movw    ON_LENGTH(%edx, %eax, 2), %ax    # countdown <-- on_length
        movw    %ax, COUNTDOWN(%edx)
        xorw    $1, STATUS(%edx)

        xorl    %eax, %eax
        movw    LOCATION(%edx), %ax     # ax <-- location
        addw    %ax, %ax                # ax <-- ax + ax (offset * 2)
        pushl   %edx
        call    mp1_poke
        popl    %edx

search_next:
        movl    NEXT(%edx), %edx
        jmp     search_begin        

search_done:
        leave                   # restore the stack frame
	ret

# int mp1_ioctl(unsigned long arg, unsigned long cmd);
#
# Description: Use the value of cmd to determine to call which core function
# Register: %eax - return value
#           %ecx - Store the cmd
# Output: -1 if cmd is invalid; 0 for success
mp1_ioctl:
        movl    $-1, %eax       # eax <-- -1 for invalid cmd
        movl    8(%esp), %ecx   # ecx <-- cmd
        cmpl    $0, %ecx        # ecx - 0
        jl      done            
        cmpl    $3, %ecx        # ecx - 3
        jg      done
        # jump to jump_table + 4 * ecx (address of core function)
        jmp    *jump_table(, %ecx, 4)  
done:
	ret

# Jump table of the mp1_ioctl
jump_table:
        .long mp1_ioctl_add, mp1_ioctl_remove, mp1_ioctl_find, mp1_ioctl_sync

# mp1_ioctl_add
#
# Description: Create a new struct, set the variable in the
#              struct, connect it to the head of link list
# Register: %eax - 1. new struct pointer
#                  2. offset of mp1_poke
#                  3. return value
#           %ecx - 1. Store the arg
#                  2. move data (temporary usage)
#                  3. %cl store the display char
#           %edx - 1. store the pointer of new struct
# Output: -1 if error, 0 for success
mp1_ioctl_add:
        pushl   %ebp            # set up stack frame
        movl    %esp, %ebp         

        cmpl    $0, 8(%ebp)     # check whether arg is a valid address
        je      arg_error

        pushl   $STRUCT_SIZE    # push the STRUCT_SIZE on stack
        call    mp1_malloc      # eax is the address of new struct
        addl    $4, %esp        # pop the STRUCT_SIZE
        pushl   %eax            # push eax to stack in case of free memory usage
        cmpl    $0, %eax        # check whether the malloc success
        je      malloc_error
  
        pushl   $STRUCT_SIZE    # mp1_copy_from_user(*to, *from, n)
        pushl   8(%ebp)
        pushl   %eax
        call    mp1_copy_from_user      # eax stores the status of memcpy
        popl    %edx                    # edx is the copied struct pointer
        addl    $8, %esp
        cmpl    $0, %eax                # check whether the copy success
        jne     malloc_error

        movw    ON_LENGTH(%edx), %cx    # countdown <-- on_length
        movw    %cx, COUNTDOWN(%edx)

        movw    $1, %cx                 # status <-- 1
        movw    %cx, STATUS(%edx)

        movl    mp1_list_head, %ecx     # next <-- mp1_list_head
        movl    %ecx, NEXT(%edx)
        leal    LOCATION(%edx), %ecx    # mp1_list_head <-- address of new struct
        movl    %ecx, mp1_list_head

        movb    ON_CHAR(%edx), %cl      # cl <-- char to display
        xorl	%eax, %eax              # eax <-- 0
        movw    LOCATION(%edx), %ax     # ax <-- offset
        cmpl    $0, %eax                # whether eax belong to [0, 1999]
        jl      malloc_error
        cmpl    $80*25-1, %eax
        jg      malloc_error
        addw    %ax, %ax                # ax <-- ax + ax (offset * 2)
        call    mp1_poke

        popl    %eax                    # pop eax (the struct pointer)
        movl    $0, %eax                # eax = 0 if success
        jmp     add_done
malloc_error:
        call    mp1_free                # eax is already on the top of the stack
        popl    %eax                    # pop eax
arg_error:
        movl    $-1, %eax               # eax = -1 if error
add_done:
        leave
	ret

# mp1_link_list_search
#
# Description: Find the pointer of a struct based on 
#              location information
# Input: %cx - desired location value
# Output: eax = null if does not exist, eax = valid address if success
#         ecx is the previous structure pointer
# Registers: Clobbers edx
mp1_link_list_search:
        pushl   %ebp
        movl    %esp, %ebp

        pushl   %ebx
        movl    $0, %ebx
        movl    mp1_list_head, %eax     # eax <-- pointer of the link list
search_start:
        cmpl    $0, %eax
        je      search_finish           # determine whether the search is failed

        movw    LOCATION(%eax), %dx     # edx <-- current location value
        cmpw    %dx, %cx
        je      search_finish
        movl    %eax, %ebx
        movl    NEXT(%eax), %eax        # eax <-- the pointer of next struct
        jmp     search_start
search_finish:     
        movl    %ebx, %ecx
        popl    %ebx    

        leave                
        ret

# mp1_ioctl_remove
#
# Description: Traverse the link list, find the struct whose location is
#              specified by arg, remove it from link list and free
# Register: %eax - 1. pointer of the found structure
#                  2. return value
#           %ecx - 1. store the arg value
#                  2. pointer of the previous structure
#           %edx = 1. data movement (temporary usage)
# Output: return -1 if error, 0 if success
mp1_ioctl_remove:
        pushl   %ebp            # set up stack frame
        movl    %esp, %ebp         

        movl    8(%ebp), %ecx           # ecx <-- arg
        call    mp1_link_list_search    # eax is the address of find struct
        cmpl    $0, %eax                # ecx is the address of previous struct
        je      remove_error

        movl    mp1_list_head, %edx
        cmpl    %edx, %eax
        je      remove_first   
        movl    NEXT(%eax), %edx        # edx <-- pointer of the next struct of delete one
        movl    %edx, NEXT(%ecx)        # remove the middle structure
        jmp     remove_free
remove_first:                           # remove the first structure
        movl    NEXT(%eax), %edx
        movl    %edx, mp1_list_head
remove_free:
        pushl   %eax                    # free the removed structure
        call    mp1_free
        popl    %eax
        movl    $0, %eax
        jmp     remove_down
remove_error:
        movl    $-1, %eax
remove_down:
        leave
	ret

# mp1_ioctl_find
#
# Description: Copy the struct to user according to the given arg
# Register: %eax - 1. store the find pointer
#                  2. act as return value
#           %ecx - 1. store the desired location
#           %edx - 1. the pointer of current struct
# Output: return -1 if error, 0 if success
mp1_ioctl_find:
        pushl   %ebp            # set up stack frame
        movl    %esp, %ebp         

        cmpl    $0, 8(%ebp)             # determine whether arg is null
        je      find_error

        pushl   $STRUCT_SIZE            # push the STRUCT_SIZE on stack
        call    mp1_malloc              # eax is the address of new struct
        addl    $4, %esp                # pop the STRUCT_SIZE
        pushl   %eax                    # push eax to stack in case of free memory usage
        cmpl    $0, %eax                # check whether the malloc success
        je      find_malloc_error

        pushl   $STRUCT_SIZE            # mp1_copy_from_user(*to, *from, n)
        pushl   8(%ebp)
        pushl   %eax
        call    mp1_copy_from_user
        popl    %edx                    # edx is the copied struct pointer
        addl    $8, %esp                # discard the arg and struct_size
        cmpl    $0, %eax                # check whether the copy success
        jne     find_malloc_error       

        movw    LOCATION(%edx), %cx     # cx <-- location
        call    mp1_link_list_search    # eax <-- return value of this function
        cmpl    $0, %eax                
        je      find_malloc_error

        pushl   $STRUCT_SIZE            # mp1_copy_to_user(*to, *from, n)
        pushl   %eax
        pushl   8(%ebp)
        call    mp1_copy_to_user
        addl    $12, %esp               # pop all the arguments
        cmpl    $0, %eax
        jne     find_malloc_error
        call    mp1_free
        movl    $0, %eax                # eax = 0 for sucess
        jmp     find_done

find_malloc_error:
        call    mp1_free
        popl    %eax                    # pop eax
find_error:
        movl    $-1, %eax               # eax = -1 for error
find_done:
        leave
	ret

# mp1_ioctl_sync
#
# Description: Take two location find two struct respectively,
#              copy the timing information from first to second
# Register: %eax - 1. struct pointer
#                  2. return value
#           %ecx - 1. store the location
#                  2. move data (temporary usage)
#                  3. store the displayed char
#           %edx - 1. store arg
#                  2. struct pointer
# Output: return -1 if error, 0 if success
mp1_ioctl_sync:
        pushl   %ebp            # set up stack frame
        movl    %esp, %ebp         

        movl    8(%ebp), %edx           # edx <-- arg
        movw    %dx, %cx                # cx <-- second location
        pushl   %edx                    # push edx for later use
        call    mp1_link_list_search    # eax is the second struct pointer
        popl    %edx
        cmpl    $0, %eax                
        je      sync_error

        shrl    $16, %edx
        movw    %dx, %cx                # cx <-- first location

        pushl   %eax                    # push the second address on stack

        call    mp1_link_list_search    # eax is the first struct pointer
        popl    %edx                    # edx is the second struct pointer
        cmpl    $0, %eax                
        je      sync_error

        movw    ON_LENGTH(%eax), %cx    # copy onlength
        movw    %cx, ON_LENGTH(%edx)
        movw    OFF_LENGTH(%eax), %cx   # copy offlength
        movw    %cx, OFF_LENGTH(%edx)
        movw    COUNTDOWN(%eax), %cx    # copy countdown
        movw    %cx, COUNTDOWN(%edx)
        movw    STATUS(%eax), %cx       # copy status
        movw    %cx, STATUS(%edx)

        xorl    %eax, %eax              # copy onchar/offchar to cl based on status
        movw    STATUS(%edx), %ax
        xorl    $1, %eax
        movb    ON_CHAR(%edx, %eax), %cl

        xorl    %eax, %eax
        movw    LOCATION(%edx), %ax
        cmpl    $0, %eax                # whether eax belong to [0, 1999]
        jl      sync_error
        cmpl    $80*25-1, %eax
        jg      sync_error
        addw    %ax, %ax
        call    mp1_poke
        movl    $0, %eax
        jmp     sync_done

sync_error:
        movl    $-1, %eax
sync_done:
        leave
	ret

.end
