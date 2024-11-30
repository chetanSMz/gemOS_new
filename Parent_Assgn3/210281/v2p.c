#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */

void invalidate_page(u64 virtual_address) {
    asm volatile("invlpg (%0)" : : "r" (virtual_address) : "memory");
}




/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{   
    if(length <= 0){
        return -1;
    }

    if(current == NULL){
        return -1;
    }

    // check whether addr is page aligned or not

    if(prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)){
        return -1;
    }


    if(length % 4096 != 0){
        length += 4096 - (length % 4096);
    }

    struct vm_area *head = current -> vm_area;
    struct vm_area *temp = head;

    while(temp -> vm_next != NULL && temp -> vm_next -> vm_end < addr){
        temp = temp -> vm_next;
    }

    struct vm_area *vm1 = temp;
    // struct vm_area *temp_print = head;


    if(vm1 -> vm_next == NULL){
        if(vm1 -> vm_end < addr){
            return 0;
        }

        // vm1 -> vm_end == addr because in this case vm1 -> vm_end > addr is not possible
        // and in case of vm1 -> vm_end == addr we do not change the pointer of vm1
        else if(vm1 -> vm_end == addr){
            return 0;
        }
    }

    else{
        if(vm1 -> vm_next -> vm_end == addr){
            vm1 = vm1 -> vm_next;
        }

        else{       // vm1 -> vm_next -> vm_end > addr
            if(vm1 -> vm_next -> vm_start < addr){      // only in this case we need to split
                vm1 = vm1 -> vm_next;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr;
                new_node -> vm_end = vm1 -> vm_end;
                new_node -> access_flags = vm1 -> access_flags;
                new_node -> vm_next = vm1 -> vm_next;
                stats -> num_vm_area++;

                vm1 -> vm_next = new_node;
                vm1 -> vm_end = addr;
            }
        }
    }

    temp = head;
    while(temp -> vm_next != NULL && temp -> vm_next -> vm_end < addr + length){
        temp = temp -> vm_next;
    }

    struct vm_area *vm2 = temp;

    if(vm2 -> vm_next == NULL){
        // do nothing
    }

    else{
        if(vm2 -> vm_next -> vm_end == addr + length){
            vm2 = vm2 -> vm_next;
        }

        else{       // vm2 -> vm_next -> vm_end > addr + length
            if(vm2 -> vm_next -> vm_start < addr + length){     // only in this case we need to split
                vm2 = vm2 -> vm_next;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr + length;
                new_node -> vm_end = vm2 -> vm_end;
                new_node -> access_flags = vm2 -> access_flags;
                new_node -> vm_next = vm2 -> vm_next;
                stats -> num_vm_area++;

                vm2 -> vm_next = new_node;
                vm2 -> vm_end = addr + length;
            }
        }
    }

    // change the access flags of all the vma's present exactly in between of vm1 and vm2 and later change
    // access flag of vm2
    temp = vm1 -> vm_next;

    while(temp != NULL && temp != vm2){
        temp -> access_flags = prot;

        u64 start_addr = temp -> vm_start;
        u64 end_addr = temp -> vm_end;
        u64 *pgd_addr = osmap(current -> pgd);

        while(start_addr < end_addr){
            u64 temp_addr = start_addr;
            u64 frame_offset = temp_addr & 0xFFF;
            temp_addr = temp_addr >> 12;
            u64 pte_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pmd_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pud_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pgd_offset = temp_addr & 0x1FF;

            u64 pgd_entry = *(pgd_addr + pgd_offset);
            if((pgd_entry & 1) != 0){   // page frame is present
                u64 pud_pfn = pgd_entry >> 12;
                u64 *pud_addr = osmap(pud_pfn);
                u64 pud_entry = *(pud_addr + pud_offset);

                if((pud_entry & 1) != 0){   // page frame is present
                    u64 pmd_pfn = pud_entry >> 12;
                    u64 *pmd_addr = osmap(pmd_pfn);
                    u64 pmd_entry = *(pmd_addr + pmd_offset);

                    if((pmd_entry & 1) != 0){
                        u64 pte_pfn = pmd_entry >> 12;
                        u64 *pte_addr = osmap(pte_pfn);
                        u64 pte_entry = *(pte_addr + pte_offset);
                        
                        if((pte_entry & 1) != 0){
                            u64 final_pfn = pte_entry >> 12;
                            invalidate_page(start_addr);
                            if(get_pfn_refcount(final_pfn) == 1){
                                if((pte_entry & 8) != 0){   // has R+W permission
                                    if(prot == PROT_READ){
                                        pte_entry -= 8;
                                        *(pte_addr + pte_offset) = pte_entry;
                                    }
                                }

                                else{       // has R permission
                                    if(prot == (PROT_READ | PROT_WRITE)){
                                        pte_entry += 8;
                                        *(pte_addr + pte_offset) = pte_entry;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            start_addr += 4096;
        }

        temp = temp -> vm_next;

    }

    if(vm2 != NULL){
        vm2 -> access_flags = prot;
        u64 start_addr = vm2 -> vm_start;
        u64 end_addr = vm2 -> vm_end;
        u64 *pgd_addr = osmap(current -> pgd);

        while(start_addr < end_addr){
            u64 temp_addr = start_addr;
            u64 frame_offset = temp_addr & 0xFFF;
            temp_addr = temp_addr >> 12;
            u64 pte_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pmd_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pud_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pgd_offset = temp_addr & 0x1FF;

            u64 pgd_entry = *(pgd_addr + pgd_offset);
            if((pgd_entry & 1) != 0){   // page frame is present
                u64 pud_pfn = pgd_entry >> 12;
                u64 *pud_addr = osmap(pud_pfn);
                u64 pud_entry = *(pud_addr + pud_offset);

                if((pud_entry & 1) != 0){   // page frame is present
                    u64 pmd_pfn = pud_entry >> 12;
                    u64 *pmd_addr = osmap(pmd_pfn);
                    u64 pmd_entry = *(pmd_addr + pmd_offset);

                    if((pmd_entry & 1) != 0){
                        u64 pte_pfn = pmd_entry >> 12;
                        u64 *pte_addr = osmap(pte_pfn);
                        u64 pte_entry = *(pte_addr + pte_offset);
                        
                        if((pte_entry & 1) != 0){
                            u64 final_pfn = pte_entry >> 12;
                            invalidate_page(start_addr);
                            if(get_pfn_refcount(final_pfn) == 1){
                                if((pte_entry & 8) != 0){   // has R+W permission
                                    if(prot == PROT_READ){
                                        pte_entry -= 8;
                                        *(pte_addr + pte_offset) = pte_entry;
                                    }
                                }

                                else{       // has R permission
                                    if(prot == (PROT_READ | PROT_WRITE)){
                                        pte_entry += 8;
                                        *(pte_addr + pte_offset) = pte_entry;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            start_addr += 4096;
        }
    }

    // now merge all the vma's which have adjacent access flags same

    temp = head;
    temp = temp -> vm_next;

    if(temp != NULL){
        while(temp -> vm_next != NULL){
            if(temp -> access_flags == temp -> vm_next -> access_flags && temp -> vm_end == temp -> vm_next -> vm_start){
                struct vm_area *free_node = temp -> vm_next;
                temp -> vm_end = temp -> vm_next -> vm_end;
                temp -> vm_next = temp -> vm_next -> vm_next;
                free_node -> vm_next = NULL;
                stats -> num_vm_area--;

                os_free(free_node, sizeof(struct vm_area));
            }
            temp = temp -> vm_next;
        }
    }

    
    return 0;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{   
    if(!(length > 0 && length <= 2 * 1024 * 1024)){         // check again
        return -1;
    }

    // how to check if the address is page aligned or not and the address can also be NULL
    // also need to check if the address + length - 1 <= MMAP_AREA_END and address >= MMAP_AREA_START
    // also need to check if the address comes in the address range of dummy node

    if(prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)){
        return -1;
    }

    if(flags != 0 && flags != MAP_FIXED){
        return -1;
    }

    if(current == NULL){
        return -1;
    }
    
    // make length to be multiple of 4kb
    if(length % 4096 != 0){
        length += 4096 - (length % 4096);
    }

    // need to check if the max num of vma's be equal to 128
    unsigned long return_addr = 0;

    if(current -> vm_area == NULL){
        struct vm_area *dummy = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        if(dummy == NULL){
            return -1;
        }
        dummy -> vm_start = MMAP_AREA_START;
        dummy -> vm_end = MMAP_AREA_START + 4096;   // can we do this ? adding a hexadecimal to an int
        dummy -> access_flags = 0;
        dummy -> vm_next = NULL;
        stats -> num_vm_area++;

        current -> vm_area = dummy;
    }

    // if addr == NULL

    if(addr == 0){
        // address is NULL and flag is MAP_FIXED
        if(flags == MAP_FIXED){
            return -1;
        }

        struct vm_area *head = current -> vm_area;
        struct vm_area *temp = head;

        while(temp -> vm_next != NULL && (temp -> vm_next -> vm_start - temp -> vm_end) < length){
            temp = temp -> vm_next;
        }

        if(temp -> vm_next == NULL){
            if(temp -> access_flags == prot && temp -> vm_start != MMAP_AREA_START){
                return_addr = temp -> vm_end;
                temp -> vm_end += length;
            }

            else{
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = temp -> vm_end;
                new_node -> vm_end = (new_node -> vm_start) + length;
                new_node -> access_flags = prot;
                new_node -> vm_next = NULL;
                stats -> num_vm_area++;

                temp -> vm_next = new_node;
                return_addr = new_node -> vm_start;
            }
        }

        else{
            if(temp -> vm_next -> vm_start - temp -> vm_end == length){
                if(temp -> access_flags == prot && temp -> vm_next -> access_flags == prot){
                    return_addr = temp -> vm_end;
                    temp -> vm_end = temp -> vm_next -> vm_end;
                    struct vm_area *free_node = temp -> vm_next;
                    temp -> vm_next = temp -> vm_next -> vm_next;

                    free_node -> vm_next = NULL;
                    os_free(free_node, sizeof(struct vm_area));
                    stats -> num_vm_area--;
                }

                else if(temp -> access_flags == prot){
                    return_addr = temp -> vm_end;
                    temp -> vm_end += length;
                }

                else if(temp -> vm_next -> access_flags == prot){
                    temp = temp -> vm_next;
                    temp -> vm_start -= length;
                    return_addr = temp -> vm_start;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = temp -> vm_end;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> vm_next = temp -> vm_next;
                    new_node -> access_flags = prot;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }

            else{
                if(temp -> access_flags == prot){
                    return_addr = temp -> vm_end;
                    temp -> vm_end += length;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = temp -> vm_end;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> access_flags = prot;
                    new_node -> vm_next = temp -> vm_next;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }
        }

    }

    // addr is not NULL

    else if(addr != 0){
        struct vm_area *head = current -> vm_area;
        struct vm_area *temp = head;

        while(temp -> vm_next != NULL && temp -> vm_next -> vm_end <= addr){
            temp = temp -> vm_next;
        }

        if(temp -> vm_next == NULL){
            if(temp -> vm_end < addr){
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr;
                new_node -> vm_end = (new_node -> vm_start) + length;
                new_node -> access_flags = prot;
                new_node -> vm_next = NULL;
                stats -> num_vm_area++;

                temp -> vm_next = new_node;
                return_addr = new_node -> vm_start;
            }

            else if(temp -> vm_end == addr){
                if(temp -> access_flags == prot && temp -> vm_start != MMAP_AREA_START){
                    return_addr = temp -> vm_end;
                    temp -> vm_end += length;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = addr;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> access_flags = prot;
                    new_node -> vm_next = NULL;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }


        }


        else if(temp -> vm_end == addr){
            if(addr + length > temp -> vm_next -> vm_start){
                if(flags == MAP_FIXED){
                    return -1;
                }

                else{
                    // find the smallest address where it can be placed
                    temp = head;
                    while(temp -> vm_next != NULL && (temp -> vm_next -> vm_start - temp -> vm_end) < length){
                        temp = temp -> vm_next;
                    }

                    if(temp -> vm_next == NULL){
                        if(temp -> access_flags == prot && temp -> vm_start != MMAP_AREA_START){
                            return_addr = temp -> vm_end;
                            temp -> vm_end += length;
                        }

                        else{
                            struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                            if(new_node == NULL){
                                return -1;
                            }
                            new_node -> vm_start = temp -> vm_end;
                            new_node -> vm_end = (new_node -> vm_start) + length;
                            new_node -> access_flags = prot;
                            new_node -> vm_next = NULL;
                            stats -> num_vm_area++;

                            temp -> vm_next = new_node;
                            return_addr = new_node -> vm_start;
                        }
                    }

                    else{
                        if(temp -> vm_next -> vm_start - temp -> vm_end == length){
                            if(temp -> access_flags == prot && temp -> vm_next -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end = temp -> vm_next -> vm_end;
                                struct vm_area *free_node = temp -> vm_next;
                                temp -> vm_next = temp -> vm_next -> vm_next;

                                free_node -> vm_next = NULL;
                                os_free(free_node, sizeof(struct vm_area));
                                stats -> num_vm_area--;
                            }

                            else if(temp -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end += length;
                            }

                            else if(temp -> vm_next -> access_flags == prot){
                                temp = temp -> vm_next;
                                temp -> vm_start -= length;
                                return_addr = temp -> vm_start;
                            }

                            else{
                                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                                if(new_node == NULL){
                                    return -1;
                                }
                                new_node -> vm_start = temp -> vm_end;
                                new_node -> vm_end = (new_node -> vm_start) + length;
                                new_node -> vm_next = temp -> vm_next;
                                new_node -> access_flags = prot;
                                stats -> num_vm_area++;

                                temp -> vm_next = new_node;
                                return_addr = new_node -> vm_start;
                            }
                        }

                        else{
                            if(temp -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end += length;
                            }

                            else{
                                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                                if(new_node == NULL){
                                    return -1;
                                }
                                new_node -> vm_start = temp -> vm_end;
                                new_node -> vm_end = (new_node -> vm_start) + length;
                                new_node -> access_flags = prot;
                                new_node -> vm_next = temp -> vm_next;
                                stats -> num_vm_area++;

                                temp -> vm_next = new_node;
                                return_addr = new_node -> vm_start;
                            }
                        }
                    }
                }
            }

            else if(addr + length == temp -> vm_next -> vm_start){
                if(temp -> access_flags == prot && temp -> vm_next -> access_flags == prot){
                    return_addr = addr;
                    temp -> vm_end = temp -> vm_next -> vm_end;
                    struct vm_area *free_node = temp -> vm_next;
                    temp -> vm_next = temp -> vm_next -> vm_next;

                    free_node -> vm_next = NULL;
                    os_free(free_node, sizeof(struct vm_area));
                    stats -> num_vm_area--;
                }

                else if(temp -> access_flags == prot){
                    return_addr = addr;
                    temp -> vm_end += length;
                }

                else if(temp -> vm_next -> access_flags == prot){
                    return_addr = addr;
                    temp = temp -> vm_next;
                    temp -> vm_start -= length;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = addr;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> access_flags = prot;
                    new_node -> vm_next = temp -> vm_next;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }

            else{
                if(temp -> access_flags == prot){
                    return_addr = temp -> vm_end;
                    temp -> vm_end += length;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = addr;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> access_flags = prot;
                    new_node -> vm_next = temp -> vm_next;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }
        }

        else if(temp -> vm_end < addr){
            if(addr + length > temp -> vm_next -> vm_start){
                if(flags == MAP_FIXED){
                    return -1;
                }

                else{
                    // find the smallest address where it can be placed
                    temp = head;
                    while(temp -> vm_next != NULL && (temp -> vm_next -> vm_start - temp -> vm_end) < length){
                        temp = temp -> vm_next;
                    }

                    if(temp -> vm_next == NULL){
                        if(temp -> access_flags == prot && temp -> vm_start != MMAP_AREA_START){
                            return_addr = temp -> vm_end;
                            temp -> vm_end += length;
                        }

                        else{
                            struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                            if(new_node == NULL){
                                return -1;
                            }
                            new_node -> vm_start = temp -> vm_end;
                            new_node -> vm_end = (new_node -> vm_start) + length;
                            new_node -> access_flags = prot;
                            new_node -> vm_next = NULL;
                            stats -> num_vm_area++;

                            temp -> vm_next = new_node;
                            return_addr = new_node -> vm_start;
                        }
                    }

                    else{
                        if(temp -> vm_next -> vm_start - temp -> vm_end == length){
                            if(temp -> access_flags == prot && temp -> vm_next -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end = temp -> vm_next -> vm_end;
                                struct vm_area *free_node = temp -> vm_next;
                                temp -> vm_next = temp -> vm_next -> vm_next;

                                free_node -> vm_next = NULL;
                                os_free(free_node, sizeof(struct vm_area));
                                stats -> num_vm_area--;
                            }

                            else if(temp -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end += length;
                            }

                            else if(temp -> vm_next -> access_flags == prot){
                                temp = temp -> vm_next;
                                temp -> vm_start -= length;
                                return_addr = temp -> vm_start;
                            }

                            else{
                                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                                if(new_node == NULL){
                                    return -1;
                                }
                                new_node -> vm_start = temp -> vm_end;
                                new_node -> vm_end = (new_node -> vm_start) + length;
                                new_node -> vm_next = temp -> vm_next;
                                new_node -> access_flags = prot;
                                stats -> num_vm_area++;

                                temp -> vm_next = new_node;
                                return_addr = new_node -> vm_start;
                            }
                        }

                        else{
                            if(temp -> access_flags == prot){
                                return_addr = temp -> vm_end;
                                temp -> vm_end += length;
                            }

                            else{
                                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                                if(new_node == NULL){
                                    return -1;
                                }
                                new_node -> vm_start = temp -> vm_end;
                                new_node -> vm_end = (new_node -> vm_start) + length;
                                new_node -> access_flags = prot;
                                new_node -> vm_next = temp -> vm_next;
                                stats -> num_vm_area++;

                                temp -> vm_next = new_node;
                                return_addr = new_node -> vm_start;
                            }
                        }
                    }
                }
            }

            else if(addr + length == temp -> vm_next -> vm_start){
                if(temp -> vm_next -> access_flags == prot){
                    temp = temp -> vm_next;
                    temp -> vm_start -= length;
                    return_addr = temp -> vm_start;
                }

                else{
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    if(new_node == NULL){
                        return -1;
                    }
                    new_node -> vm_start = addr;
                    new_node -> vm_end = (new_node -> vm_start) + length;
                    new_node -> vm_next = temp -> vm_next;
                    new_node -> access_flags = prot;
                    stats -> num_vm_area++;

                    temp -> vm_next = new_node;
                    return_addr = new_node -> vm_start;
                }
            }

            else{
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr;
                new_node -> vm_end = (new_node -> vm_start) + length;
                new_node -> vm_next = temp -> vm_next;
                new_node -> access_flags = prot;
                stats -> num_vm_area++;

                temp -> vm_next = new_node;
                return_addr = new_node -> vm_start;
            }
        }
    }

    return return_addr;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{   
    if(length <= 0){
        return -1;
    }

    if(current == NULL){
        return -1;
    }

    // check whether addr is page aligned or not

    if(length % 4096 != 0){
        length += 4096 - (length % 4096);
    }

    struct vm_area *head = current -> vm_area;
    struct vm_area *temp = head;

    while(temp -> vm_next != NULL && temp -> vm_next -> vm_end < addr){
        temp = temp -> vm_next;
    }

    struct vm_area *vm1 = temp;

    if(vm1 -> vm_next == NULL){
        if(vm1 -> vm_end < addr){
            return 0;
        }

        // vm1 -> vm_end == addr because in this case vm1 -> vm_end > addr is not possible
        // and in case of vm1 -> vm_end == addr we do not change the pointer of vm1
        else if(vm1 -> vm_end == addr){
            return 0;
        }
    }

    else{
        if(vm1 -> vm_next -> vm_end == addr){
            vm1 = vm1 -> vm_next;
        }

        else{       // vm1 -> vm_next -> vm_end > addr
            if(vm1 -> vm_next -> vm_start < addr){      // only in this case we need to split
                vm1 = vm1 -> vm_next;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr;
                new_node -> vm_end = vm1 -> vm_end;
                new_node -> access_flags = vm1 -> access_flags;
                new_node -> vm_next = vm1 -> vm_next;
                stats -> num_vm_area++;

                vm1 -> vm_next = new_node;
                vm1 -> vm_end = addr;
            }
        }
    }

    temp = head;

    while(temp -> vm_next != NULL && temp -> vm_next -> vm_end < addr + length){
        temp = temp -> vm_next;
    }

    struct vm_area *vm2 = temp;

    if(vm2 -> vm_next == NULL){
        // do nothing
    }

    else{
        if(vm2 -> vm_next -> vm_end == addr + length){
            vm2 = vm2 -> vm_next;
        }

        else{       // vm2 -> vm_next -> vm_end > addr + length
            if(vm2 -> vm_next -> vm_start < addr + length){     // only in this case we need to split
                vm2 = vm2 -> vm_next;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                if(new_node == NULL){
                    return -1;
                }
                new_node -> vm_start = addr + length;
                new_node -> vm_end = vm2 -> vm_end;
                new_node -> access_flags = vm2 -> access_flags;
                new_node -> vm_next = vm2 -> vm_next;
                stats -> num_vm_area++;

                vm2 -> vm_next = new_node;
                vm2 -> vm_end = addr + length;
            }
        }
    }

    while(vm1 -> vm_next != NULL && vm1 -> vm_next != vm2){
        struct vm_area *free_node = vm1 -> vm_next;
        vm1 -> vm_next = vm1 -> vm_next -> vm_next;
        free_node -> vm_next = NULL;
        stats -> num_vm_area--;
        u64 start_addr = free_node -> vm_start;
        u64 end_addr = free_node -> vm_end;

        u64 *pgd_addr = osmap(current -> pgd);

        while(start_addr < end_addr){
            u64 temp_addr = start_addr;
            u64 frame_offset = temp_addr & 0xFFF;
            temp_addr = temp_addr >> 12;
            u64 pte_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pmd_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pud_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pgd_offset = temp_addr & 0x1FF;

            u64 pgd_entry = *(pgd_addr + pgd_offset);
            if((pgd_entry & 1) != 0){   // page frame is present
                u64 pud_pfn = pgd_entry >> 12;
                u64 *pud_addr = osmap(pud_pfn);
                u64 pud_entry = *(pud_addr + pud_offset);

                if((pud_entry & 1) != 0){   // page frame is present
                    u64 pmd_pfn = pud_entry >> 12;
                    u64 *pmd_addr = osmap(pmd_pfn);
                    u64 pmd_entry = *(pmd_addr + pmd_offset);

                    if((pmd_entry & 1) != 0){
                        u64 pte_pfn = pmd_entry >> 12;
                        u64 *pte_addr = osmap(pte_pfn);
                        u64 pte_entry = *(pte_addr + pte_offset);

                        if((pte_entry & 1) != 0){
                            invalidate_page(start_addr);
                            u64 final_pfn = pte_entry >> 12;
                            put_pfn(final_pfn);
                            if(get_pfn_refcount(final_pfn) == 0){
                                os_pfn_free(USER_REG, final_pfn);
                            }
                            pte_entry = 0;
                            *(pte_addr + pte_offset) = pte_entry;
                        }
                    }
                }
            }
            start_addr += 4096;
        }

        os_free(free_node, sizeof(struct vm_area));

    }

    if(vm2 != NULL){
        vm1 -> vm_next = vm2 -> vm_next;
        vm2 -> vm_next = NULL;
        stats -> num_vm_area--;
        u64 start_addr = vm2 -> vm_start;
        u64 end_addr = vm2 -> vm_end;

        u64 *pgd_addr = osmap(current -> pgd);

        while(start_addr < end_addr){
            u64 temp_addr = start_addr;
            u64 frame_offset = temp_addr & 0xFFF;
            temp_addr = temp_addr >> 12;
            u64 pte_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pmd_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pud_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pgd_offset = temp_addr & 0x1FF;

            u64 pgd_entry = *(pgd_addr + pgd_offset);
            if((pgd_entry & 1) != 0){   // page frame is present
                u64 pud_pfn = pgd_entry >> 12;
                u64 *pud_addr = osmap(pud_pfn);
                u64 pud_entry = *(pud_addr + pud_offset);

                if((pud_entry & 1) != 0){   // page frame is present
                    u64 pmd_pfn = pud_entry >> 12;
                    u64 *pmd_addr = osmap(pmd_pfn);
                    u64 pmd_entry = *(pmd_addr + pmd_offset);

                    if((pmd_entry & 1) != 0){
                        u64 pte_pfn = pmd_entry >> 12;
                        u64 *pte_addr = osmap(pte_pfn);
                        u64 pte_entry = *(pte_addr + pte_offset);

                        if((pte_entry & 1) != 0){
                            invalidate_page(start_addr);
                            u64 final_pfn = pte_entry >> 12;
                            put_pfn(final_pfn);
                            if(get_pfn_refcount(final_pfn) == 0){
                                os_pfn_free(USER_REG, final_pfn);
                            }
                            pte_entry = 0;
                            *(pte_addr + pte_offset) = pte_entry;
                        }
                    }
                }
            }
            start_addr += 4096;
        }

        os_free(vm2, sizeof(struct vm_area));

    }

    return 0;
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{   
    if(current == NULL){
        return -1;
    }

    struct vm_area *head = current -> vm_area;
    struct vm_area *temp = head;

    // if the addr is present bw MMAP_AREA_START and MMAP_AREA_START + 4095, should we mark it as invalid?
    temp = temp -> vm_next;
    int valid = 0;
    while(temp != NULL){
        if(addr >= temp -> vm_start && addr < temp -> vm_end){
            valid = 1;
            if(temp -> access_flags == PROT_READ && (error_code & 2) != 0){
                valid = 0;
            }
            break;
        }
        temp = temp -> vm_next;
    }

    if(!valid){
        return -1;
    }

    if((error_code & 1) != 0 && (error_code & 2) != 0){
        handle_cow_fault(current, addr, temp -> access_flags);
    }

    u64 *pgd_addr = osmap(current -> pgd); // virtual address of pgd table

    u64 temp_addr = addr;
    u64 frame_offset = temp_addr & 0xFFF;
    temp_addr = temp_addr >> 12;
    u64 pte_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pmd_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pud_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pgd_offset = temp_addr & 0x1FF;

    int alloc_pmd = 0;
    int alloc_pte = 0;
    int alloc_final = 0;

    if((error_code & 1) == 0){         // no physical mapping is present
        // allocate pfn for addr
        u64 pgd_entry = *(pgd_addr + pgd_offset);
        if((pgd_entry & 1) == 0){     // allocate a pfn for pud
            u32 pud_pfn = os_pfn_alloc(OS_PT_REG);
            pgd_entry = pud_pfn << 12;
            pgd_entry += 1;     // making the present bit = 1;
            pgd_entry += 8;
            pgd_entry += 16;    // making the page frame accessible from user_mode
            alloc_pmd = 1;
        }
        *(pgd_addr + pgd_offset) = pgd_entry;

        u64 pud_pfn = pgd_entry >> 12;
        u64 *pud_addr = osmap(pud_pfn);
        u64 pud_entry = *(pud_addr + pud_offset);

        if(alloc_pmd == 1 || (pud_entry & 1) == 0){     // allocate a pfn for pud
            u32 pmd_pfn = os_pfn_alloc(OS_PT_REG);
            pud_entry = pmd_pfn << 12;
            pud_entry += 1;     // making the present bit = 1;
            pud_entry += 8;
            pud_entry += 16;    // making the page frame accessible from user_mode
            alloc_pte = 1;
        }
        *(pud_addr + pud_offset) = pud_entry;

        u64 pmd_pfn = pud_entry >> 12;
        u64 *pmd_addr = osmap(pmd_pfn);
        u64 pmd_entry = *(pmd_addr + pmd_offset);

        if(alloc_pte == 1 || (pmd_entry & 1) == 0){     // allocate a pfn for pud
            u32 pte_pfn = os_pfn_alloc(OS_PT_REG);
            pmd_entry = pte_pfn << 12;
            pmd_entry += 1;     // making the present bit = 1;
            pmd_entry += 8;
            pmd_entry += 16;    // making the page frame accessible from user_mode
            alloc_final = 1;
        }
        *(pmd_addr + pmd_offset) = pmd_entry;

        u64 pte_pfn = pmd_entry >> 12;
        u64 *pte_addr = osmap(pte_pfn);
        u64 pte_entry = *(pte_addr + pte_offset);

        if(alloc_final == 1 || (pte_entry & 1) == 0){
            u32 final_pfn = os_pfn_alloc(USER_REG);
            pte_entry = final_pfn << 12;
            pte_entry += 1;     // making the present bit = 1
            if(temp -> access_flags == (PROT_READ | PROT_WRITE)){
                pte_entry += 8;
            }
            pte_entry += 16;    // making the page frame accessible from user mode
        }
        *(pte_addr + pte_offset) = pte_entry;
    }
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
    new_ctx -> ppid = ctx -> pid;
    new_ctx -> type = ctx -> type;
    new_ctx -> state = ctx -> state;
    new_ctx -> used_mem = ctx -> used_mem;
    new_ctx -> os_rsp = ctx -> os_rsp;
    for(int i = 0; i < MAX_MM_SEGS; i++){
        new_ctx -> mms[i] = ctx -> mms[i];
    }
    new_ctx -> vm_area = ctx -> vm_area;
    for(int i = 0; i < CNAME_MAX; i++){
        new_ctx -> name[i] = ctx -> name[i];
    }
    new_ctx -> regs = ctx -> regs;
    new_ctx -> pending_signal_bitmap = ctx -> pending_signal_bitmap;
    for(int i = 0; i < MAX_SIGNALS; i++){
        new_ctx -> sighandlers[i] = ctx -> sighandlers[i];
    }
    new_ctx -> ticks_to_alarm = ctx -> ticks_to_alarm;
    new_ctx -> ticks_to_sleep = ctx -> ticks_to_sleep;
    new_ctx -> alarm_config_time = ctx -> alarm_config_time;
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        new_ctx -> files[i] = ctx -> files[i];
    }
    new_ctx -> ctx_threads = ctx -> ctx_threads;

    // create page table for the child process
    u64 pgd_pfn = os_pfn_alloc(OS_PT_REG);
    u64 *pgd_addr = osmap(pgd_pfn);
    new_ctx -> pgd = pgd_pfn;

    struct vm_area *head = new_ctx -> vm_area;
    struct vm_area *temp = head;

    for(int i = 0; i < MAX_MM_SEGS; i++){
    u64 start_addr = new_ctx -> mms[i].start;
    u64 end_addr = 0;
    if(i != MM_SEG_STACK){
        end_addr = new_ctx -> mms[i].next_free;
    }
    
    else{
        end_addr = new_ctx -> mms[i].end;
    }

    while(start_addr < end_addr){
        int alloc_pud = 0;
        int alloc_pmd = 0;
        int alloc_pte = 0;

        u64 temp_addr = start_addr;
        u64 frame_offset = temp_addr & 0xFFF;
        temp_addr = temp_addr >> 12;
        u64 pte_offset = temp_addr & 0x1FF;
        temp_addr = temp_addr >> 9;
        u64 pmd_offset = temp_addr & 0x1FF;
        temp_addr = temp_addr >> 9;
        u64 pud_offset = temp_addr & 0x1FF;
        temp_addr = temp_addr >> 9;
        u64 pgd_offset = temp_addr & 0x1FF;

        u64 *parent_pgd_addr = osmap(ctx -> pgd);
            u64 parent_pgd_entry = *(parent_pgd_addr + pgd_offset);
            if((parent_pgd_entry & 1) != 0){   // page frame is present
                u64 parent_pud_pfn = parent_pgd_entry >> 12;
                u64 *parent_pud_addr = osmap(parent_pud_pfn);
                u64 parent_pud_entry = *(parent_pud_addr + pud_offset);

                if((parent_pud_entry & 1) != 0){   // page frame is present
                    u64 parent_pmd_pfn = parent_pud_entry >> 12;
                    u64 *parent_pmd_addr = osmap(parent_pmd_pfn);
                    u64 parent_pmd_entry = *(parent_pmd_addr + pmd_offset);

                    if((parent_pmd_entry & 1) != 0){
                        u64 parent_pte_pfn = parent_pmd_entry >> 12;
                        u64 *parent_pte_addr = osmap(parent_pte_pfn);
                        u64 parent_pte_entry = *(parent_pte_addr + pte_offset);
                        
                        if((parent_pte_entry & 1) != 0){

                            u64 pgd_entry = *(pgd_addr + pgd_offset);
                            if((pgd_entry & 1) == 0){
                                u32 pud_pfn = os_pfn_alloc(OS_PT_REG);
                                pgd_entry = pud_pfn << 12;
                                pgd_entry += 1;     // making the present bit = 1;
                                pgd_entry += 8;     // R + W
                                pgd_entry += 16;    // making the page frame accessible from user_mode
                                alloc_pud = 1;
                            }

                            *(pgd_addr + pgd_offset) = pgd_entry;

                            u64 pud_pfn = pgd_entry >> 12;
                            u64 *pud_addr = osmap(pud_pfn);
                            u64 pud_entry = *(pud_addr + pud_offset);

                            if(alloc_pud == 1 || (pud_entry & 1) == 0){     // allocate a pfn for pud
                                u32 pmd_pfn = os_pfn_alloc(OS_PT_REG);
                                pud_entry = pmd_pfn << 12;
                                pud_entry += 1;     // making the present bit = 1;
                                pud_entry += 8;     // R + W
                                pud_entry += 16;    // making the page frame accessible from user_mode
                                alloc_pmd = 1;
                            }
                            *(pud_addr + pud_offset) = pud_entry;

                            u64 pmd_pfn = pud_entry >> 12;
                            u64 *pmd_addr = osmap(pmd_pfn);
                            u64 pmd_entry = *(pmd_addr + pmd_offset);

                            if(alloc_pmd == 1 || (pmd_entry & 1) == 0){     // allocate a pfn for pud
                                u32 pte_pfn = os_pfn_alloc(OS_PT_REG);
                                pmd_entry = pte_pfn << 12;
                                pmd_entry += 1;     // making the present bit = 1;
                                pmd_entry += 8;     // R + W
                                pmd_entry += 16;    // making the page frame accessible from user_mode
                                alloc_pte = 1;
                            }
                            *(pmd_addr + pmd_offset) = pmd_entry;

                            u64 pte_pfn = pmd_entry >> 12;
                            u64 *pte_addr = osmap(pte_pfn);
                            u64 pte_entry = *(pte_addr + pte_offset);

                            if(alloc_pte == 1 || (pte_entry & 1) == 0){
                                if((parent_pte_entry & 8) != 0){   // has R+W permission
                                    parent_pte_entry -= 8;
                                    *(parent_pte_addr + pte_offset) = parent_pte_entry;
                                }

                                *(pte_addr + pte_offset) = parent_pte_entry;

                                // increase the refcount of the page frame
                                u32 final_pfn = parent_pte_entry >> 12;
                                get_pfn(final_pfn);                                
                            }
                        }
                    }
                }
            }        
            
            start_addr += 4096;
        }     
    }

    while(temp != NULL){
        u64 start_addr = temp -> vm_start;
        u64 end_addr = temp -> vm_end;

        while(start_addr < end_addr){
            u64 temp_addr = start_addr;
            u64 frame_offset = temp_addr & 0xFFF;
            temp_addr = temp_addr >> 12;
            u64 pte_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pmd_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pud_offset = temp_addr & 0x1FF;
            temp_addr = temp_addr >> 9;
            u64 pgd_offset = temp_addr & 0x1FF;

            u64 pgd_entry = *(pgd_addr + pgd_offset);

            if((pgd_entry & 1) == 0){
                u32 pud_pfn = os_pfn_alloc(OS_PT_REG);
                pgd_entry = pud_pfn << 12;
                pgd_entry += 1;     // making the present bit = 1;
                pgd_entry += 8;     // R + W
                pgd_entry += 16;    // making the page frame accessible from user_mode
            }

            *(pgd_addr + pgd_offset) = pgd_entry;

            u64 pud_pfn = pgd_entry >> 12;
            u64 *pud_addr = osmap(pud_pfn);
            u64 pud_entry = *(pud_addr + pud_offset);

            if((pud_entry & 1) == 0){     // allocate a pfn for pud
                u32 pmd_pfn = os_pfn_alloc(OS_PT_REG);
                pud_entry = pmd_pfn << 12;
                pud_entry += 1;     // making the present bit = 1;
                pud_entry += 8;     // R + W
                pud_entry += 16;    // making the page frame accessible from user_mode
            }
            *(pud_addr + pud_offset) = pud_entry;

            u64 pmd_pfn = pud_entry >> 12;
            u64 *pmd_addr = osmap(pmd_pfn);
            u64 pmd_entry = *(pmd_addr + pmd_offset);

            if((pmd_entry & 1) == 0){     // allocate a pfn for pud
                u32 pte_pfn = os_pfn_alloc(OS_PT_REG);
                pmd_entry = pte_pfn << 12;
                pmd_entry += 1;     // making the present bit = 1;
                pmd_entry += 8;     // R + W
                pmd_entry += 16;    // making the page frame accessible from user_mode
            }
            *(pmd_addr + pmd_offset) = pmd_entry;

            u64 pte_pfn = pmd_entry >> 12;
            u64 *pte_addr = osmap(pte_pfn);
            u64 pte_entry = *(pte_addr + pte_offset);

            if((pte_entry & 1) == 0){
                u64 *parent_pgd_addr = osmap(ctx -> pgd);
                u64 parent_pgd_entry = *(parent_pgd_addr + pgd_offset);
                if((parent_pgd_entry & 1) != 0){   // page frame is present
                    u64 parent_pud_pfn = parent_pgd_entry >> 12;
                    u64 *parent_pud_addr = osmap(parent_pud_pfn);
                    u64 parent_pud_entry = *(parent_pud_addr + pud_offset);

                    if((parent_pud_entry & 1) != 0){   // page frame is present
                        u64 parent_pmd_pfn = parent_pud_entry >> 12;
                        u64 *parent_pmd_addr = osmap(parent_pmd_pfn);
                        u64 parent_pmd_entry = *(parent_pmd_addr + pmd_offset);

                        if((parent_pmd_entry & 1) != 0){
                            u64 parent_pte_pfn = parent_pmd_entry >> 12;
                            u64 *parent_pte_addr = osmap(parent_pte_pfn);
                            u64 parent_pte_entry = *(parent_pte_addr + pte_offset);
                            
                            if((parent_pte_entry & 1) != 0){
                                if((parent_pte_entry & 8) != 0){   // has R+W permission
                                    parent_pte_entry -= 8;
                                    *(parent_pte_addr + pte_offset) = parent_pte_entry;
                                }

                                *(pte_addr + pte_offset) = parent_pte_entry;

                                // increase the refcount of the page frame
                                u32 final_pfn = parent_pte_entry >> 12;
                                get_pfn(final_pfn);
                            }
                        }
                    }
                }
            }

            start_addr += 4096;
        }        
        
        temp = temp -> vm_next;
    }
    
    new_ctx -> pgd = pgd_pfn;

     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{   
    if(current == NULL){
        return -1;
    }

    u64 *pgd_addr = osmap(current -> pgd);
    u64 temp_addr = vaddr;
    u64 frame_offset = temp_addr & 0xFFF;
    temp_addr = temp_addr >> 12;
    u64 pte_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pmd_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pud_offset = temp_addr & 0x1FF;
    temp_addr = temp_addr >> 9;
    u64 pgd_offset = temp_addr & 0x1FF;

    u64 pgd_entry = *(pgd_addr + pgd_offset);
    if((pgd_entry & 1) != 0){   // page frame is present
        u64 pud_pfn = pgd_entry >> 12;
        u64 *pud_addr = osmap(pud_pfn);
        u64 pud_entry = *(pud_addr + pud_offset);

        if((pud_entry & 1) != 0){   // page frame is present
            u64 pmd_pfn = pud_entry >> 12;
            u64 *pmd_addr = osmap(pmd_pfn);
            u64 pmd_entry = *(pmd_addr + pmd_offset);

            if((pmd_entry & 1) != 0){
                u64 pte_pfn = pmd_entry >> 12;
                u64 *pte_addr = osmap(pte_pfn);
                u64 pte_entry = *(pte_addr + pte_offset);

                if((pte_entry & 1) != 0){
                    u64 final_pfn = pte_entry >> 12;
                    u64 *final_pfn_addr = osmap(final_pfn);

                    if((pte_entry & 8) == 0 && access_flags == (PROT_READ | PROT_WRITE)){
                        u64 new_pfn = os_pfn_alloc(USER_REG);
                        u64 *new_pfn_addr = osmap(new_pfn);

                        memcpy(new_pfn_addr, final_pfn_addr, 4096);
                        u64 new_entry = new_pfn << 12;
                        new_entry += 1;
                        if(access_flags == (PROT_READ | PROT_WRITE)){
                            new_entry += 8;   // check
                        }
                        new_entry += 16;
                        *(pte_addr + pte_offset) = new_entry;

                        // decrease the ref count of original_pfn
                        put_pfn(final_pfn);

                        // if ref_count of pfn is 0, free pfn
                        if(get_pfn_refcount(final_pfn) == 0){       // check
                            os_pfn_free(USER_REG, final_pfn);
                        }
                    }
                }
            }
        }
    }


    return 1;
}

