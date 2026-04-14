#include "user_poll.h"
#include "poll_manager.h"
#include <stdlib.h>

void creatRS485Tast_time(uint16_t address, uint16_t *value);

// 工具函数：插入排序
void sort_addresses(uint16_t *arr, int size)
{
    for (int i = 1; i < size; i++)
    {
        uint16_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// 合并允许间隔的地址块
int merge_with_gap(uint16_t *addr_list, int addr_count, AddrBlock *blocks)
{
    if (addr_count == 0)
        return 0;

    sort_addresses(addr_list, addr_count); // 先排序地址

    int block_count = 0;
    uint16_t current_start = addr_list[0];
    uint16_t current_end = addr_list[0];

    for (int i = 1; i < addr_count; i++)
    {
        if (addr_list[i] - current_end <= MAX_MERGE_GAP)
        {
            current_end = addr_list[i]; // 合并到当前块
        }
        else
        {
            // 保存当前块
            blocks[block_count].start_addr = current_start;
            blocks[block_count].length = current_end - current_start + 1;
            block_count++;
            // 开始新块
            current_start = addr_list[i];
            current_end = addr_list[i];
        }
    }
    // 保存最后一个块
    blocks[block_count].start_addr = current_start;
    blocks[block_count].length = current_end - current_start + 1;
    return block_count + 1;
}

// 分割超长块
int split_blocks(AddrBlock *merged, int merged_count, AddrBlock *requests, MenuItemType type, MenuItemType *output_types)
{
    int req_count = 0;
    if (!merged_count)
        return 0;

    for (int i = 0; i < merged_count; i++)
    {
        uint16_t start = merged[i].start_addr;
        uint16_t total_len = merged[i].length;
        uint16_t offset = 0;

        while (offset < total_len)
        {
            uint16_t chunk_len = (total_len - offset > MAX_READ_LENGTH) ? MAX_READ_LENGTH : (total_len - offset);
            requests[req_count].start_addr = start + offset;
            requests[req_count].length = chunk_len;
            output_types[req_count] = type; // 标记类型
            req_count++;
            offset += chunk_len;
            if (req_count >= MAX_REQUESTS)
                break;
        }
    }
    return req_count;
}

// 生成请求队列入口函数
int generate_requests(uint16_t *addr_list, MenuItemType *type_list, int addr_count, AddrBlock *output, MenuItemType *output_types)
{
    // 分离位和字地址
    uint16_t bit_addrs[MAX_ADDR_LIST_SIZE], word_addrs[MAX_ADDR_LIST_SIZE];
    int bit_count = 0, word_count = 0;

    for (int i = 0; i < addr_count; i++)
    {
        if (type_list[i] == MENU_TYPE_VALUE_BIT)
        {
            bit_addrs[bit_count++] = addr_list[i];
        }
        else if (type_list[i] == MENU_TYPE_VALUE_INT || type_list[i] == MENU_TYPE_VALUE_UINT16)
        {
            word_addrs[word_count++] = addr_list[i];
        }
        else if (type_list[i] == MENU_TYPE_VALUE_DINT || type_list[i] == MENU_TYPE_VALUE_UINT32)
        {
            word_addrs[word_count++] = addr_list[i]; // 双字地址需要+1
            word_addrs[word_count++] = addr_list[i] + 1;
        }
    }
    // 步骤1：合并地址块
    AddrBlock merged_bits[MAX_MERGED_BLOCKS];
    int merged_bit_count = merge_with_gap(bit_addrs, bit_count, merged_bits);
    AddrBlock merged_words[MAX_MERGED_BLOCKS];
    int merged_word_count = merge_with_gap(word_addrs, word_count, merged_words);
    // 步骤2：分割超长块
    int req_count = 0;
    req_count += split_blocks(merged_bits, merged_bit_count, output + req_count, MENU_TYPE_VALUE_BIT, output_types + req_count);
    req_count += split_blocks(merged_words, merged_word_count, output + req_count, MENU_TYPE_VALUE_INT, output_types + req_count);
    return req_count;
}

// 轮询任务设置
void add_menu_polltask(void)
{
    static enum {
        REQ_IDLE = 0,
        REQ_CREATE,
    } req_state;

    static AddrBlock requests[MAX_REQUESTS];
    static MenuItemType request_types[MAX_REQUESTS]; // 保存每个请求的类型
    static uint8_t req_count = 0;
    static uint8_t new_req = 0;
    // static MB_ReqType req_type;
    MB_Request req;

    switch (req_state)
    {
    case REQ_IDLE:
    {
        memset(requests, 0, sizeof(requests));
        memset(request_types, 0, sizeof(request_types));
        /// test_type = (test_type == POLL_CURRENT_PAGE)? POLL_ALWAYS : POLL_CURRENT_PAGE;
        // 构造读请求：当前页相关寄存器
        uint16_t addresses[MAX_ADDR_LIST_SIZE];
        MenuItemType types[MAX_ADDR_LIST_SIZE];
        uint8_t addr_count = PollManager_GetPollingAddresses(addresses, types, 200); // TODO此处获取需要完善该功能
        if (addr_count > 0)
        {
            req_count = generate_requests(addresses, types, addr_count, requests, request_types);
        }
        else
        {
            return;
        }
        req_state = REQ_CREATE;
        break;
    }
    case REQ_CREATE:
    {
        // 构造读请求
        while (new_req < req_count)
        {
            req.type = (request_types[new_req] == MENU_TYPE_VALUE_INT) ? REQ_READ_HOLDING_REGISTERS : REQ_READ_COILS;

            req.slave_addr = SLAVE_ADDR;
            req.reg_addr = requests[new_req].start_addr;
            req.data_len = requests[new_req].length;

            if (MBController_Request(req) == REQ_ADD_FAILURE)
            {
                return;
            }
            new_req++;
        }

        new_req = 0;
        req_count = 0;
        memset(requests, 0, sizeof(requests));
        memset(request_types, 0, sizeof(request_types));
        req_state = REQ_IDLE;
        break;
    }
    default:
        break;
    }
}
void add_menu_polltask_setTime(void)
{
}
void creatRS485Tast_time(uint16_t address, uint16_t *value)
{
}

void WriteValue(uint16_t addr, uint8_t width, int32_t value)
{
    MB_Request req;
    req.slave_addr = SLAVE_ADDR;

    if (width == 1) // 位装置
    {
        req.type = REQ_WRITE_SINGLE_COIL;
        req.reg_addr = addr;
        req.len_of_data = 1;

        uint16_t *data = (uint16_t *)malloc(sizeof(uint16_t));
        if (data == NULL)
            return; // 内存分配失败
        *data = (value != 0) ? 0xFF00 : 0x0000;
        req.data = data;
    }
    else if (width == 2) // 16位字
    {
        req.type = REQ_WRITE_SINGLE_REGISTER;
        req.reg_addr = addr;
        req.len_of_data = 1;

        uint16_t *data = (uint16_t *)malloc(sizeof(uint16_t));
        if (data == NULL)
            return;
        *data = (uint16_t)value;
        req.data = data;
    }
    else if (width == 4) // 32位双字
    {
        req.type = REQ_WRITE_MULTIPLE_REGISTERS;
        req.reg_addr = addr;
        req.len_of_data = 2;

        uint16_t *data = (uint16_t *)malloc(2 * sizeof(uint16_t));
        if (data == NULL)
            return;
        data[0] = (uint16_t)(value & 0xFFFF);
        data[1] = (uint16_t)((value >> 16) & 0xFFFF);
        req.data = data;
    }
    else
    {
        return;
    }

    MBController_Request(req);
}