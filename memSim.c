#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256 
#define TLB_SIZE 16

int *page_table;
signed char *physical_memory;
int num_frames;
int next_free_frame = 0;

int tlb_page[TLB_SIZE];
int tlb_frame[TLB_SIZE];
int tlb_index = 0;

int search_tlb(int page_number){
	int i;
	for(i = 0; i < TLB_SIZE; i++){
		if(tlb_page[i] = page_number)
			return tlb_frame[i];
	}
	return -1;
}

void add_to_tlb(int page_number, int frame_number){
	tlb_page[tlb_index] = page_number;
	tlb_frame[tlb_index] = frame_number;
	tlb_index = (tlb_index + 1) % TLB_SIZE;	
}

int handle_page_fault(int page_number, FILE *backing_store){
	if(next_free_frame >= num_frames){
		fprintf(stderr, "No free frames available! Page replacement needed.\n");
		exit(1);
	}

	if(fseek(backing_store, page_number * PAGE_SIZE, SEEK_SET) != 0){
		fprintf(stderr, "Error seeking in backing store.\n");
		exit(1);
	}

	if(fread(&physical_memory[next_free_frame * PAGE_SIZE], sizeof(signed char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
		fprintf(stderr, "Error reading from backing store.\n");
		exit(1);	
	}

	page_table[page_number] = next_free_frame;
	return next_free_frame++;
}

int main(int argc, char *argv[]){
	if(argc != 3){
		fprintf(stderr, "Usage: %s <num_frames> <addresses_file>\n", argv[0]);
		exit(1);
	}

	num_frames = atoi(argv[1]);
	const char *address_file_name = argv[2];

	if(num_frames <= 0 || num_frames > NUM_PAGES){
		fprintf(stderr, "Invalid frame count. Must be between 1 and %d.\n", NUM_PAGES);
		exit(1);
	}

	page_table = malloc(NUM_PAGES * sizeof(int));
	int j;
	int k;
	for(j = 0; j < NUM_PAGES; j++){
		page_table[j] = -1;
	}

	for(k = 0; k < TLB_SIZE; k++){
		tlb_page[k] = -1;
		tlb_frame[k] = -1;
	}
	
	FILE *backing_store = fopen("Backing_STORE.bin", "rb");

	if(!backing_store){
		perror("Error opening BACKING_STORE.bin");
		exit(1); 
	}

	FILE *addr_file = fopen(address_file_name, "r");
	if(!addr_file) {
		perror("Error opening address file");
		exit(1);
	}

	int logical_address;
	int tlb_his = 0, pages_faults = 0, total_addresses = 0;

	while(fscanf(addr_file, "%d", &logical_address) == 1){
		total_addresses++;
		int page_number = (logical_address >> 8) & 0xFF;
		int offset = logical_address & 0xFF;
		int frame_number = search_tlb(page_number);

		if(frame_number != -1){
			tlb_hits++;
		}else{
			frame_number = page_table[page_number];

			if(frame_number == -1){
				page_faults++;
				frame_number = handle_page_fault(page_number, backing_store);
			}
	
		add_to_tlb(page_number, frame_number);
	
		}

	int physical_address = (frame_number << 8) | offset;
	signed char value = physical_memory[physical_address % (num_frames * PAGE_SIZE)];

	printf("Logical Addr: %5d | Physical Addr: %5d | Value: %d\n", logical_address, physical_address, value);

	}
	printf("\n=== Summary ===\n");
	printf("Addresses Translated: %d\n", total_addresses);
	printf("TLB Hits: %d\n", tlb_hits);
	printf("Page Faults: %d\n", page_faults);
	printf("TLB Hit Rate: %.2f%%\n",(tlb_hits / (float)total_addresses) * 100);
	printf("Page Fault Rate: %.2f%%\n", (page_faults / (float)total_addresses) * 100);

	fclose(backing_store);
	fclose(addr_file);
	free(page_table);
	free(physical_memory);	
	return 0;
}



























