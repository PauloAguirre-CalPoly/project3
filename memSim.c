#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 256
#define NUM_PAGES 256 
#define TLB_SIZE 16

typedef struct{
	int frame;
	int valid;
	int last_used;
}PageTableEntry;

int num_frames = 256;
char replacement_alg[10] = "fifo";

int tlb_misses = 0;
int next_free_frame = 0;
int tlb_page[TLB_SIZE];
int tlb_frame[TLB_SIZE];
int tlb_index = 0;

PageTableEntry page_table[NUM_PAGES];
signed char *physical_memory;

int *frame_to_page;

int tlb_hits = 0;
int page_faults = 0;
int total_addresses = 0;

void print_frame(int frame_number);
int search_tlb(int page_number);
void addTotlb(int page_number, int frame_number);
int handle_page_fault(int page_number, FILE *backing_store, int *ref_seq, int ref_count, int current_index);
int fifo_replacement();
int lru_replacement();
int opt_replacement(int *ref_seq, int ref_count, int current_index);
void replace_page(int page_number, FILE *backing_store, int *ref_seq, int ref_count, int current_index);

void print_frame(int frame_number){
	if(frame_number < 0 || frame_number >= num_frames){
		printf("Invalid frame number %d\n", frame_number);
		return;
	}
	
	unsigned char *frame_ptr = (unsigned char*)&physical_memory[frame_number * PAGE_SIZE];
	int i;
	for(i = 0; i <PAGE_SIZE; i++){
		printf("%02X", frame_ptr[i]);
	}
//	printf("\n");
}

int search_tlb(int page_number){
	int i;
	for(i = 0; i < TLB_SIZE; i++){
		if(tlb_page[i] == page_number)
			return tlb_frame[i];
	}
	return -1;
}

void addTotlb(int page_number, int frame_number){
	tlb_page[tlb_index] = page_number;
	tlb_frame[tlb_index] = frame_number;
	tlb_index = (tlb_index + 1) % TLB_SIZE;	
}

int handle_page_fault(int page_number, FILE *backing_store, int *ref_seq, int ref_count, int current_index){
	page_faults++;
	int frame_number;
	if(next_free_frame < num_frames){
		frame_number = next_free_frame++;
	}else{


		if(strcmp(replacement_alg, "fifo") == 0){
			frame_number = fifo_replacement();
		}
		else if(strcmp(replacement_alg, "lru") == 0){
			frame_number = lru_replacement();
		}else{
			frame_number = opt_replacement(ref_seq, ref_count, current_index);
		}

	}

	if(fseek(backing_store, page_number * PAGE_SIZE, SEEK_SET) != 0){
		perror("Error seeking in backing store.");
		exit(1);
	}
	
	if(fread(&physical_memory[frame_number * PAGE_SIZE], sizeof(signed char), PAGE_SIZE, backing_store) != PAGE_SIZE) {
		perror("Error reading from backing store.");
		exit(1);	
	}

	page_table[page_number].frame = frame_number;
	page_table[page_number].valid = 1;
	page_table[page_number].last_used = total_addresses;

	frame_to_page[frame_number] = page_number;

	return frame_number;
}

int fifo_index = 0;

int fifo_replacement(){
	int page_to_replace = frame_to_page[fifo_index];
	page_table[page_to_replace].valid = 0;
	int replaced_frame = fifo_index;
	fifo_index = (fifo_index + 1) % num_frames;
	return replaced_frame;
}

int lru_replacement(){
	int lru_page = -1;
	int lru_time = 1e9;
	int p;
	for(p =0; p < NUM_PAGES; p++){
		if(page_table[p].valid && page_table[p].last_used < lru_time){
			lru_page = p;
			lru_time = page_table[p].last_used;
		}
	}
	int frame = page_table[lru_page].frame;
	page_table[lru_page].valid = 0;
	return frame;
}

int opt_replacement(int *ref_seq, int ref_count, int current_index){
	int farthest_use = -1;
	int page_to_replace = -1;

	int p;
	
	for(p = 0; p < NUM_PAGES; p++){
		if(!page_table[p].valid){
			continue;
		}


	
		int found_future = 0;
		int i;
		for(i = current_index + 1; i < ref_count; i++){
			
			int page_i = (ref_seq[i] >> 8) & 0xFF;
				//if(ref_seq[i] == p){
			if(page_i == p){		
			found_future = 1;
				if(i > farthest_use){
					farthest_use = i;
					page_to_replace = p;
				}
			break;
			}
		}
		if(!found_future){
			page_to_replace = p;
			break;
		}
	}
	int frame = page_table[page_to_replace].frame;
	page_table[page_to_replace].valid = 0;
	return frame;
}

int main(int argc, char *argv[]){
	if(argc < 2){
		fprintf(stderr, "Usage: %s <reference-sequence.txt> [FRAMES] [PRA]\n", argv[0]);
		exit(1);
	}

	const char *address_file_name = argv[1];
	if(argc >= 3){
		num_frames = atoi(argv[2]);
	} 

	if(argc >= 4){
		strncpy(replacement_alg, argv[3], sizeof(replacement_alg)-1);
		replacement_alg[sizeof(replacement_alg) - 1] = '\0';
	}

	if(num_frames <= 0 || num_frames > NUM_PAGES){
		fprintf(stderr, "Invalid frame count (1-256).\n");
		exit(1);
	}

	physical_memory = malloc(num_frames * PAGE_SIZE * sizeof(signed char));
	frame_to_page = malloc(num_frames * sizeof(int));

	int l;
	for(l = 0; l < num_frames; l++){
		frame_to_page[l] = -1;
	}

	int j;
	for(j = 0; j < NUM_PAGES; j++){
		page_table[j].valid = 0;
		page_table[j].frame = -1;
		page_table[j].last_used = 0;
	}

	int k;
	for(k = 0; k < TLB_SIZE; k++){
		tlb_page[k] = -1;
		tlb_frame[k] = -1;
	}
	
	FILE *backing_store = fopen("BACKING_STORE.bin", "rb");

	if(!backing_store){
		perror("Error opening BACKING_STORE.bin");
		exit(1); 
	}

	FILE *addr_file = fopen(address_file_name, "r");
	if(!addr_file) {
		perror("Error opening address file");
		exit(1);
	}

	int cap = 1000;
	int *ref_seq = malloc(cap * sizeof(int));
	if(ref_seq == NULL){
		fprintf(stderr, "Error allocating reference sequence.\n");
		exit(1);
	}

	int addr;
	int ref_count = 0;

	while(fscanf(addr_file, "%d", &addr) == 1){
		if(ref_count >= cap){
			cap *= 2;
			ref_seq = realloc(ref_seq, cap * sizeof(int));
			if(ref_seq == NULL){
				fprintf(stderr, "Error reallocating reference sequence.\n");
				exit(1);		
}}

		ref_seq[ref_count++] = addr;
	}

	rewind(addr_file);

	fifo_index = 0;

	int logical_address;
	//int loopCount = 0;
	while(fscanf(addr_file, "%d", &logical_address) == 1){
		total_addresses++;
		int page_number = (logical_address >> 8) & 0xFF;
		int offset = logical_address & 0xFF;
		int frame_number = search_tlb(page_number);

		if(frame_number != -1){
			tlb_hits++;
		}else{
			tlb_misses++;
			if(!page_table[page_number].valid){
				frame_number = handle_page_fault(page_number, backing_store, ref_seq, ref_count,  total_addresses-1);
			}else{
				frame_number = page_table[page_number].frame;
			}
	
			addTotlb(page_number, frame_number);
	
		}
		
		page_table[page_number].last_used = total_addresses;
		signed char value = physical_memory[frame_number * PAGE_SIZE + offset];

//		printf("%d, %d, %d,\n", logical_address, value, frame_number);
		printf("%d, %d, %d, ", logical_address, (int)value, frame_number);
		print_frame(frame_number);
	//	loopCount++;
		printf("\n");
	}
//	printf("\n");
	printf("Number of Translated Addresses = %d\n", total_addresses);
	printf("Page Faults = %d\n", page_faults);
	printf("Page Fault Rate = %.3f\n", (page_faults / (float)total_addresses));
	printf("TLB Hits = %d\n", tlb_hits);
	printf("TLB Misses = %d\n", tlb_misses);
	printf("TLB Hit Rate = %.3f\n",(tlb_hits / (float)total_addresses) * 100);

	fclose(backing_store);
	fclose(addr_file);
	free(ref_seq);
	free(physical_memory);	
	free(frame_to_page);

	return 0;
}

