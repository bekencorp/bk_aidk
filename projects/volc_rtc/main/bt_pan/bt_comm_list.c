#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "bt_comm_list.h"

#define TAG "bt_comm_list"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


bt_comm_list_t *bt_comm_list_new(void)
{
    bt_comm_list_t *list = (bt_comm_list_t *) psram_zalloc(sizeof(bt_comm_list_t));
    if (!list) {
        return NULL;
    }

    list->head = list->tail = NULL;
    list->length = 0;
    return list;
}

bool bt_comm_list_is_empty(const bt_comm_list_t *list)
{
    return (list->length == 0);
}

static bt_comm_list_node_t *bt_comm_list_free_node(bt_comm_list_t *list, bt_comm_list_node_t *node)
{
    bt_comm_list_node_t *next = node->next;

    psram_free(node->data);

    psram_free(node);
    --list->length;

    return next;
}

void bt_comm_list_clear(bt_comm_list_t *list)
{
    for (bt_comm_list_node_t *node = list->head; node; ) {
        node = bt_comm_list_free_node(list, node);
    }
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
}

void bt_comm_list_free(bt_comm_list_t *list)
{
    if (!list) {
        return;
    }

    bt_comm_list_clear(list);
    psram_free(list);
}

bool bt_comm_list_append(bt_comm_list_t *list, void *data)
{
    bt_comm_list_node_t *node = (bt_comm_list_node_t *)psram_zalloc(sizeof(bt_comm_list_node_t));
    if (!node) {
        LOGE("%s psram_zalloc failed.\n", __func__ );
        return false;
    }
    node->next = NULL;
    node->data = data;
    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    ++list->length;
    return true;
}

void *bt_comm_list_front(const bt_comm_list_t *list)
{
    return list->head->data;
}

bool bt_comm_list_remove(bt_comm_list_t *list, void *data)
{
    if (bt_comm_list_is_empty(list)) {
        return false;
    }

    if (list->head->data == data) {
        bt_comm_list_node_t *next = bt_comm_list_free_node(list, list->head);
        if (list->tail == list->head) {
            list->tail = next;
        }
        list->head = next;
        return true;
    }

    for (bt_comm_list_node_t *prev = list->head, *node = list->head->next; node; prev = node, node = node->next)
    {
        if (node->data == data) {
            prev->next = bt_comm_list_free_node(list, node);
            if (list->tail == node) {
                list->tail = prev;
            }
            return true;
        }
    }
    return false;
}

