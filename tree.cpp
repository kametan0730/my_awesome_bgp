#include <cstdio>
#include <cstdlib>

#include "tree.h"

bool check_bit(uint32_t addr, uint8_t n){
    return (addr >> (31 - n)) & 0b01;
}

void print_address_binary(uint32_t addr){
    for(int i = 0; i < 32; ++i){
        if(check_bit(addr, i)){
            printf("1");
        }else{
            printf("0");
        }
    }
    printf("\n");
}

void print_address_binary_2(uint32_t addr){
    for(int i = 31; i >= 0; --i){
        if(check_bit(addr, i)){
            printf("1");
        }else{
            printf("0");
        }
    }
    printf("\n");
}

void assert_tree(node* node){
    if(node->node_0 != nullptr){
        if(node->node_0->prefix_len != node->prefix_len + 1){
            printf("Invalid prefix length %d and %d\n", node->node_0->prefix_len, node->prefix_len);
        }else{
            printf("Assert OK\n");
        }
        assert_tree(node->node_0);
    }
    if(node->node_1 != nullptr){
        if(node->node_1->prefix_len != node->prefix_len + 1){
            printf("Invalid prefix length %d and %d\n", node->node_1->prefix_len, node->prefix_len);
        }else{
            printf("Assert OK\n");
        }
        assert_tree(node->node_1);
    }
}

/**
 *
 * @param prefix 削除するノード
 * @param is_delete_child_prefix Trueの場合は子要素をすべて削除する
 */
void delete_prefix(node* prefix, bool is_delete_child_prefix){
    prefix->is_prefix = false; // 削除対象のプレフィックスはプレフィックス扱いしない
    prefix->next_hop = 0;
    if(!is_delete_child_prefix and (prefix->node_1 != nullptr or prefix->node_0 != nullptr)){
        return;
    }
    if(is_delete_child_prefix and prefix->node_1 != nullptr and prefix->node_0 == nullptr){
        return delete_prefix(prefix->node_1, true);
    }
    if(is_delete_child_prefix and prefix->node_0 != nullptr and prefix->node_1 == nullptr){
        return delete_prefix(prefix->node_0, true);
    }
    node* tmp;
    if(is_delete_child_prefix and prefix->node_1 != nullptr and prefix->node_0 != nullptr){
        tmp = prefix->node_1;
        prefix->node_1 = nullptr;
        delete_prefix(prefix, true);
        return delete_prefix(tmp, true);
    }
    node* current = prefix;
    while(!current->is_prefix and current->parent != nullptr and (current->parent->node_0 == nullptr or current->parent->node_1 == nullptr)){
        tmp = current->parent;
        if(current->parent->node_1 == current){
            current->parent->node_1 = nullptr;
        }else{
            current->parent->node_0 = nullptr;
        }
        free(current);
        current = tmp;
    }
}

/**
 *
 * @param root 検索を行う木構造の根
 * @param address 検索するアドレス
 * @param max_prefix_len 検索するノードの最大プレフィックス長
 * @param is_prefix_strict Trueの場合はmax_prefix_lenのプレフィックス長のノードが見つからなければnullptrを返す
 * @return　
 */
node* search_prefix(node* root, uint32_t address, uint8_t max_prefix_len, bool is_prefix_strict){
    node* current = root;
    node* next;
    node* match_node = root;
    uint8_t i = 0;
    while(i < max_prefix_len){
        next = (check_bit(address, i) ? current->node_1 : current->node_0);
        if(next == nullptr){
            if(is_prefix_strict){
                return nullptr;
            }
            return match_node;
        }
        if(next->is_prefix){
            match_node = next;
        }
        i++;
        current = next;
    }
    return match_node;
}

/**
 *
 * @param root 追加を行う木構造の根
 * @param prefix 追加するアドレスプレフィックス
 * @param prefix_len 追加するアドレスプレフィックスのプレフィックス長
 * @param next_hop 追加するネクストホップ
 * @return
 */
node* add_prefix(node* root, uint32_t prefix, uint8_t prefix_len, uint32_t next_hop){
    node* current = search_prefix(root, prefix, prefix_len-1);
    uint8_t res_prefix_len = current->prefix_len;
    res_prefix_len++;
    node** growth_address_ptr;
    while(res_prefix_len < prefix_len){ // 枝を伸ばす
        if(check_bit(prefix, res_prefix_len-1)){
            growth_address_ptr = &current->node_1;
        }else{
            growth_address_ptr = &current->node_0;
        }
        if((*growth_address_ptr) != nullptr){
            current = *growth_address_ptr;
        }else{
            node* growth_node = (node*) malloc(sizeof(node));
            growth_node->is_prefix = false;
            growth_node->prefix = current->prefix;
            growth_node->prefix_len = res_prefix_len;
            growth_node->next_hop = 0;
            growth_node->parent = current;
            growth_node->node_0 = nullptr;
            growth_node->node_1 = nullptr;
            if(check_bit(prefix, res_prefix_len - 1)){
                growth_node->prefix |= (0x01 << (res_prefix_len));
                current->node_1 = growth_node;
            }else{
                current->node_0 = growth_node;
            }
            current = growth_node;
        }
        res_prefix_len++;
    }

    if(check_bit(prefix, prefix_len - 1)){
        growth_address_ptr = &current->node_1;
    }else{
        growth_address_ptr = &current->node_0;
    }
    if((*growth_address_ptr) == nullptr){
        node* new_prefix = (node*) malloc(sizeof(node));
        new_prefix->is_prefix = true;
        new_prefix->prefix = prefix;
        new_prefix->prefix_len = prefix_len;
        new_prefix->next_hop = next_hop;
        new_prefix->parent = current;
        new_prefix->node_0 = nullptr;
        new_prefix->node_1 = nullptr;
        *growth_address_ptr = new_prefix;
    }else{
        (*growth_address_ptr)->is_prefix = true;
        (*growth_address_ptr)->next_hop = next_hop;
    }

    return *growth_address_ptr;
}