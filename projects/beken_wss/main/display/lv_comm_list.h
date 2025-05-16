/**
 * @file lv_comm_list.h
 *
 */

#ifndef LV_COMM_LIST_H
#define LV_COMM_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct lv_comm_list_node_t {
    struct lv_comm_list_node_t *next;
    void *data;
};

typedef struct lv_comm_list_node_t lv_comm_list_node_t;

typedef struct lv_comm_list_t {
    lv_comm_list_node_t *head;
    lv_comm_list_node_t *tail;
    uint16_t length;
} lv_comm_list_t;

lv_comm_list_t *lv_comm_list_new(void);
bool lv_comm_list_is_empty(const lv_comm_list_t *list);
void *lv_comm_list_front(const lv_comm_list_t *list);
bool lv_comm_list_remove(lv_comm_list_t *list, void *data);
void lv_comm_list_clear(lv_comm_list_t *list);
void lv_comm_list_free(lv_comm_list_t *list);
bool lv_comm_list_append(lv_comm_list_t *list, void *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_COMM_LIST_H*/

