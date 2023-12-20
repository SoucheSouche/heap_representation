/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps_init.h"
#include "esp_heap_caps.h"
#include "heap_memory_layout.h"

#include "helper.h"

static color_t current_color = color_unknown;

void app_main(void)
{
    printf("main application started!\n");

    const size_t heap_memory_size = ROW * COLUMN;
    
    void* heap_memory = malloc(heap_memory_size);
    memset(heap_memory, 0x00, heap_memory_size);

    char* heap_representation = malloc(heap_memory_size);
    char* prev_representation = malloc(heap_memory_size);
    memset(heap_representation, DEFAULT_USED_ASCII, heap_memory_size);
    memset(prev_representation, 0, heap_memory_size);

    const uint32_t test_cap = 1 << 30;
    const uint32_t test_caps[SOC_MEMORY_TYPE_NO_PRIOS] = { test_cap };

    const esp_err_t ret_val = heap_caps_add_region_with_caps(test_caps,
                                                             (intptr_t)heap_memory,
                                                             (intptr_t)(heap_memory + heap_memory_size));

    if (ret_val != ESP_OK) {
        printf("couldn't create new heap region: %d\n", ret_val);
        return;
    }

    control_t* control = (control_t*)(heap_memory + 20);

    // mark the heap_t bytes, tlsf metadata and sentinel block
    memset(heap_representation, HEAP_T_ASCII, sizeof(heap_t));
    memset(heap_representation + HEAP_T_SIZE, METADATA_ASCII, control->size);
    memset(heap_representation + heap_memory_size - 4, BLOCK_HEADER_ASCII, 4);

    const size_t number_of_pointers = PTR_NUMBER;
    uint32_t ptr_array[number_of_pointers];
    for (int i = 0; i < number_of_pointers; i++)
    {
        ptr_array[i] = 0;
    }

    clear_screen();

    info_t information;
    init_info(&information);
size_t counter = 0;
    while (true)
    {
        update_heap_representation(control, heap_memory, heap_representation, heap_memory_size);
        print_update_heap_representation(heap_representation, prev_representation, heap_memory_size);
        print_alloc_info(information);

        size_t ptr_nb = rand() % number_of_pointers;
        if (ptr_array[ptr_nb] != 0)
        {
            esp_cpu_cycle_count_t start = esp_cpu_get_cycle_count();
            heap_caps_free((uint32_t*)ptr_array[ptr_nb]);
            esp_cpu_cycle_count_t end = esp_cpu_get_cycle_count();
            information.average_free_ticks = (information.average_free_ticks + end - start) >> 1;
            ptr_array[ptr_nb] = 0;
        }
        else
        {
            size_t alloc_size = (rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE)) + MIN_ALLOC_SIZE;
            if (counter == 1) {
                alloc_size = 128;
            }
            counter++;
            esp_cpu_cycle_count_t start = esp_cpu_get_cycle_count();
            void* ptr = heap_caps_malloc(alloc_size, test_cap);
            esp_cpu_cycle_count_t end = esp_cpu_get_cycle_count();
            information.average_alloc_ticks = (information.average_alloc_ticks + end - start) >> 1;
            if (ptr == NULL)
            {
                information.alloc_miss++;
                information.size_of_last_failed = alloc_size;
            }
            else
            {
                ptr_array[ptr_nb] = (intptr_t)ptr;

                // store the requested size in the first bytes of allocated memory
                // to allow retrieving this information when creating the heap representation
                memcpy(ptr, &alloc_size, sizeof(size_t));
            }
        }

        // // update the previous representation
        memcpy(prev_representation, heap_representation, heap_memory_size);

        vTaskDelay(100);
    }
}

void update_heap_representation(control_t* control, void* heap_memory, char* heap_representation, size_t heap_memory_size)
{
    block_header_t* header = (block_header_t*)(heap_memory + HEAP_T_SIZE + control->size - 4);

    bool exit = false;
    while (exit != true)
    {
        uint32_t relative_offset = (intptr_t)header - (intptr_t)heap_memory;
        size_t size_of_block = block_size(header);

        if (block_is_free(header))
        {
        #ifndef CONFIG_HEAP_POISONING_DISABLED
            // check one by one the bytes to see if the fill pattern is set to free
            for (int i = 0; i < size_of_block - 8; i++) {
                if (*((uint8_t*)heap_memory + relative_offset + 16 + i) == FREE_FILL_PATTERN) {
                    heap_representation[relative_offset + 16 + i] = POISON_FREE_ASCII;
                }
                else {
                    heap_representation[relative_offset + 16 + i] = DEFAULT_FREE_ASCII;
                }
            }
        #else
            memset(heap_representation + relative_offset + 16, DEFAULT_FREE_ASCII, size_of_block - 8);
        #endif
            memset(heap_representation + relative_offset + 4, BLOCK_HEADER_ASCII, 12);
            memset(heap_representation + relative_offset + 4 + size_of_block, BLOCK_FOOTER_ASCII, 4);
        }
        else
        {
            memset(heap_representation + relative_offset + 4, BLOCK_HEADER_ASCII, 4);

        #ifndef CONFIG_HEAP_POISONING_DISABLED
            poison_head_t* poison_head = (poison_head_t*)((uint8_t*)header + sizeof(poison_head_t));
            const size_t size = size_of_block == 0 ? 0 : poison_head->alloc_size;
            memset(heap_representation + relative_offset + 8 + sizeof(poison_head_t) + size, CANARY_ASCII, sizeof(poison_tail_t));
            memset(heap_representation + relative_offset + 8, CANARY_ASCII, sizeof(poison_head_t));

            // check one by one the bytes to see if the fill pattern is set to alloc
            size_t offset = relative_offset + 8 + sizeof(poison_head_t);
            for (int i = 0; i < size; i++) {
                if (*((uint8_t*)heap_memory + offset + i) == MALLOC_FILL_PATTERN) {
                    heap_representation[offset + i] = POISON_USED_ASCII;
                }
                else {
                    heap_representation[offset + i] = DEFAULT_USED_ASCII;
                }
            }

            const size_t remaining_size = size == 0 ? 0 : size_of_block - size - POISON_OVERHEAD;
            memset(heap_representation + relative_offset + 8 + size + POISON_OVERHEAD, PADDING_ASCII, remaining_size);
        #else
            size_t size = 0;
            if (size_of_block != 0) {
                memcpy(&size, (uint8_t*)header + 8, sizeof(size_t));
                memset(heap_representation + relative_offset + 8, DEFAULT_USED_ASCII, size);
                memset(heap_representation + relative_offset + 8 + size, PADDING_ASCII, size_of_block - size);
            }
        #endif
        }
        
        header = (block_header_t*)((uint8_t*)header + block_size(header) + 4);

        if (block_size(header) == 0)
        {
            exit = true;
        }
    }
}

void print_update_heap_representation(char* current, char* previous, size_t size)
{
    for (int i = 0; i < size; i++) {
        uint8_t current_char = *(current + i);
        uint8_t previous_char = *(previous + i);
        if (current_char != previous_char) {
            // position the cursor at the right place
            size_t row = i / COLUMN + 1;
            size_t col = i % COLUMN + 1;
            printf("\033[%d;%dH", row, col);

            // write the character
            if ((current_char == BLOCK_HEADER_ASCII || current_char == BLOCK_FOOTER_ASCII)
                && (current_color != color_blue)) {
                set_print_color(color_blue, true); // set and update the current color
            }
            else if ((current_char == DEFAULT_USED_ASCII || current_char == POISON_USED_ASCII)
                     && (current_color != color_red)) {
                set_print_color(color_red, true); // set and update the current color
            }
            else if ((current_char == DEFAULT_FREE_ASCII || current_char == POISON_FREE_ASCII)
                     && (current_color != color_green)) {
                set_print_color(color_green, true); // set and update the current color
            }
            else if ((current_char == CANARY_ASCII)
                    && (current_color != color_yellow)) {
                set_print_color(color_yellow, true); // set and update the current color
            }
            else if ((current_char == HEAP_T_ASCII || current_char == METADATA_ASCII || current_char == PADDING_ASCII)
                    && (current_color != color_default)) {
                set_print_color(color_default, true); // set and update the current color
            }
            else {
                // nothing to do here
            }

            putchar(current_char); // send the character
        }
    }
}

void print_alloc_info(info_t info)
{
    static int overflow_counter = 0;
    static int runtime_prev = 0;
    static int runtime = 0;
    int runtime_cur = esp_cpu_get_cycle_count() / (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * MHZ_TO_HZ);
    if (runtime_cur < runtime_prev)
    {
        overflow_counter++;
        runtime_cur += 26 - runtime_prev;
    }
    runtime = runtime_cur + 26 * overflow_counter;
    runtime_prev = runtime_cur;

    // set color without updating the current color
    set_print_color(color_default, false);

    printf("\033[%d;%dH", 5, COLUMN + 5);
    printf("runtime: %.2d:%.2d:%.2d    ", runtime / 3600, (runtime / 60) % 60, runtime % 60);
    printf("\033[%d;%dH", 6, COLUMN + 5);
    printf("number of failed allocation: %d    ", info.alloc_miss);
    printf("\033[%d;%dH", 7, COLUMN + 5);
    printf("last failed alloc size: %d    ", info.size_of_last_failed);

    printf("\033[%d;%dH", 8, COLUMN + 5);
    printf("Ticks per free: %d    ", info.average_free_ticks);
    printf("\033[%d;%dH", 9, COLUMN + 5);
    printf("Ticks per alloc: %d    ", info.average_alloc_ticks);

    // set color without updating the current color
    set_print_color(current_color, false);
}

void set_print_color(const color_t color, const bool update_current_color) {
    if (color == color_blue) {
        printf("\033[0;34m");
    }
    else if (color == color_red) {
        printf("\033[0;31m");
    }
    else if (color == color_green) {
        printf("\033[32;1m");
    }
    else if (color == color_yellow) {
        printf("\033[0;33m");
    }
    else if (color == color_default) {
        printf("\033[0m");
    }
    else {
        // nothing to do
    }

    if (update_current_color) {
        current_color = color;
    }
}