# Memory virtualization in User space
score 90/100
Single page allocation - 25/25, 
Multi page allocation - 15/25, 
TLB - 20/20, 
Multi-threading support - 10/10, 
Larger PGSIZE support - 10/10, 
Report - 10/10, 
4-level page table (extra credit) 0/5, 
Reducing fragmentation 0/10 
Total - 90/100 
Additional Comments 
- allocating memory of size > page_size and doing put_value/get_value segfaults (we use SIZE=33, ARRAY_SIZE=4356) in test.c
