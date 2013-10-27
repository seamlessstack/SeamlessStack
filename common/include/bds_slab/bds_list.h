/*
 * Double linked list implementation for BlueDS 
 *
 * Copyright 2011 Shubhro Sinha
 * Author : Shubhro Sinha <shubhro@shubhalok.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; Version 2 of the License.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */

#ifndef _BDS_LIST_H_
#define _BDS_LIST_H_

static inline void INIT_LIST_HEAD (bds_list_head_t head)
{
	head->next = head->prev = head;
}

/**
 * Insert a new entry between two known consecutive entries 
 * This is only for internal list manipulation if we know 
 * prev/next before hand
 */
static inline void __bds_list_add (bds_list_head_t new,
			       bds_list_head_t prev, 
			       bds_list_head_t next)
{
	prev->next = new;
	new->next = next;
	new->prev = prev;
	next->prev = new;
}
/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */

static inline void bds_list_add (bds_list_head_t new, bds_list_head_t head)
{
	__bds_list_add (new, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */

static inline void bds_list_add_tail (bds_list_head_t new, bds_list_head_t head)
{
	__bds_list_add (new, head->prev,head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __bds_list_del(bds_list_head_t prev, bds_list_head_t next)
{
	next->prev = prev;
	prev->next = next;
}
/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void __bds_list_del_entry(bds_list_head_t entry)
{
	 __bds_list_del(entry->prev, entry->next);
}

static inline void bds_list_del(bds_list_head_t entry)
{
	__bds_list_del(entry->prev, entry->next);
}

/* List iteration macros */

/**
 * list_entry: Get the struct for this entry
 * @ptr:	pointer to the node
 * @type:	the type of struct where the list_struct is embedded
 * @member: 	the name of the list_struct embedded in the struct
 */
#define list_entry(ptr, type, member) \
	container_of(ptr,type,member)

/**
 * list_for_each: iterate over a list
 * @pos:	the &struct list to use as the loop cursor
 * @head:	the head for your list	
 */
#define list_for_each(pos, head) \
	for (pos=(head)->next; pos != head; pos = pos->next)

/**
 * list_first_entry - get the first element from a list
 * @ptr:        the list head to take the element from.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	(ptr->next != ptr->prev) ? list_entry(ptr, type, member): NULL
/**
 * list_for_each_entry  -       iterate over list of given type
 * @pos:        the type * to use as a loop cursor.
 * @head:       the head for your list.
 * @member:     the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)                          \
        for (pos = list_entry((head)->next, typeof(*pos), member);      \
             &pos->member != (head);    \
             pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * bds_list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const bds_list_head_t head)
{
        return head->next == head;
}

#endif /* _BDS_LIST_H_ */
