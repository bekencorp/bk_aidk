/**
 * @file bt_comm_list.h
 *
 */

#ifndef BT_COMM_LIST_H
#define BT_COMM_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct bt_comm_list_node_t {
    struct bt_comm_list_node_t *next;
    void *data;
};

typedef struct bt_comm_list_node_t bt_comm_list_node_t;

typedef struct bt_comm_list_t {
    bt_comm_list_node_t *head;
    bt_comm_list_node_t *tail;
    uint16_t length;
} bt_comm_list_t;

bt_comm_list_t *bt_comm_list_new(void);
bool bt_comm_list_is_empty(const bt_comm_list_t *list);
void *bt_comm_list_front(const bt_comm_list_t *list);
bool bt_comm_list_remove(bt_comm_list_t *list, void *data);
void bt_comm_list_clear(bt_comm_list_t *list);
void bt_comm_list_free(bt_comm_list_t *list);
bool bt_comm_list_append(bt_comm_list_t *list, void *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*BT_COMM_LIST_H*/
