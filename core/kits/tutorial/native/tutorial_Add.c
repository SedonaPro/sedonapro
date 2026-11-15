#include "sedona.h"
#include "list.h"
#include <stdlib.h>


LIST_HEAD(tutorial_addrs);

struct addr_data{ 
	uint8_t* component;
	uint32_t* addr;
	struct list_head list;
};

void printListHead(struct list_head *list)
{
	printf("next = %p\n", list->next);
	printf("prev = %p\n", list->prev);
}

void printAddrData(struct addr_data *data)
{
	printf("component = %p\n", data->component);
	printf("addr = %p\n",      data->addr);
	printListHead(&data->list);
}


Cell tutorial_Add_printList(SedonaVM* vm, Cell* params) {
}


Cell tutorial_Add_rememberAddress(SedonaVM* vm, Cell* params) {
	uint8_t* self = params[0].aval;   /* 'this' pointer is implicit
				           * first element of params[] */
	uint8_t* slot = params[1].aval;
	uint16_t offset = getSlotHandle(vm, slot);
	
	struct addr_data *ptr = malloc(sizeof(*ptr));
	if (!ptr) {
		fprintf(stderr, "tutorial_Add_rememberAddress: failed allocate memory\n");
		return falseCell;
	}

	ptr->component = self;
	ptr->addr = getInline(self, offset);
	list_add(&ptr->list, &tutorial_addrs);
	return trueCell;
}


Cell tutorial_Add_checkAddress(SedonaVM* vm, Cell* params) {
	uint8_t* self = params[0].aval;
	struct addr_data *ptr;
	Cell ret = negOneCell;

	
	list_for_each_entry(ptr, &tutorial_addrs, list) {
		if (ptr->component == self) {
			ret.ival = ptr->addr ? ++(*ptr->addr) : -1;
			break;
		}
	}

	return ret;
}

Cell tutorial_Add_forgetAddress(SedonaVM* vm, Cell* params) {
	uint8_t* self = params[0].aval;
	struct addr_data *ptr;
	Cell ret = negOneCell;

	list_for_each_entry(ptr, &tutorial_addrs, list) {
		if (ptr->component == self) {
			ret = trueCell;
			list_del(&ptr->list);
			break;
		}
	}
	
	return ret;
}


