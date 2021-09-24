#ifndef TEST_RIB_TREE_TREE_H
#define TEST_RIB_TREE_TREE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

template <typename DATA_TYPE>
struct node{
    bool is_prefix = false;
    uint32_t prefix = 0;
    uint8_t prefix_len = 0;
    DATA_TYPE* data = nullptr;
    node* parent = nullptr;
    node* node_0 = nullptr;
    node* node_1 = nullptr;
};

bool check_bit(uint32_t addr, uint8_t n);
void print_address_binary(uint32_t addr);

template <typename DATA_TYPE>
void assert_tree(node<DATA_TYPE>* node){
    if(node->node_0 != nullptr){
        if(node->node_0->prefix_len != node->prefix_len + 1){
            printf("Invalid prefix length %d and %d\n", node->node_0->prefix_len, node->prefix_len);
        }
        assert_tree(node->node_0);
    }
    if(node->node_1 != nullptr){
        if(node->node_1->prefix_len != node->prefix_len + 1){
            printf("Invalid prefix length %d and %d\n", node->node_1->prefix_len, node->prefix_len);
        }
        assert_tree(node->node_1);
    }
}

/**
 *
 * @param prefix 削除するノード
 * @param is_delete_child_prefix Trueの場合は子要素をすべて削除する
 */
template <typename DATA_TYPE>
void delete_prefix(node<DATA_TYPE>* prefix, bool is_delete_child_prefix = false){
    prefix->is_prefix = false; // 削除対象のプレフィックスはプレフィックス扱いしない
    if(prefix->data == nullptr){
        free(prefix->data);
        prefix->data = nullptr;
    }
    if(!is_delete_child_prefix and (prefix->node_1 != nullptr or prefix->node_0 != nullptr)){
        return;
    }
    if(is_delete_child_prefix and prefix->node_1 != nullptr and prefix->node_0 == nullptr){
        return delete_prefix(prefix->node_1, true);
    }
    if(is_delete_child_prefix and prefix->node_0 != nullptr and prefix->node_1 == nullptr){
        return delete_prefix(prefix->node_0, true);
    }
    node<DATA_TYPE>* tmp;
    if(is_delete_child_prefix and prefix->node_1 != nullptr and prefix->node_0 != nullptr){
        /*
        tmp = prefix->node_1; tmpをnode_1にするとなぜか全経路削除の時に free(): double free detected in tcache 2 でAbortedする
        prefix->node_1 = nullptr; 追記: いや、今でも発生する
        */
        tmp = prefix->node_0;
        tmp->parent = nullptr;
        prefix->node_0 = nullptr;
        delete_prefix(prefix, true);
        return delete_prefix(tmp, true);
    }
    node<DATA_TYPE>* current = prefix;
    while(!current->is_prefix and current->parent != nullptr and (current->parent->node_0 == nullptr or current->parent->node_1 == nullptr)){
#ifdef TEST_RIB_TREE_TEST_TREE
        printf("Release: %s/%d, %p\n", inet_ntoa(in_addr{.s_addr = htonl(current->prefix)}), current->prefix_len, current);
#endif
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
template <typename DATA_TYPE>
node<DATA_TYPE>* search_prefix(node<DATA_TYPE>* root, uint32_t address, uint8_t max_prefix_len = 32, bool is_prefix_strict = false){
    node<DATA_TYPE>* current = root;
    node<DATA_TYPE>* next = nullptr;
    node<DATA_TYPE>* match_node = root;
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
        current = next;
        i++;
    }
    if(is_prefix_strict and max_prefix_len != 0 and match_node->prefix_len == 0){
        return nullptr;
    }
    return match_node;
}

/**
 *
 * @param root 追加を行う木構造の根
 * @param prefix 追加するアドレスプレフィックス
 * @param prefix_len 追加するアドレスプレフィックスのプレフィックス長
 * @param data 経路につける情報
 * @param is_updated 呼び出し元に経路の更新だったのか追加だったのかを知らせる
 * @return
 */
template <typename DATA_TYPE>
node<DATA_TYPE>* add_prefix(node<DATA_TYPE>* root, uint32_t prefix, uint8_t prefix_len, DATA_TYPE data, bool* is_updated = nullptr){
    node<DATA_TYPE>* current = search_prefix(root, prefix, prefix_len-1);
    uint8_t current_prefix_len = current->prefix_len;
#ifdef TEST_RIB_TREE_TEST_TREE
    printf("Get prefix: %d\n", current_prefix_len);
#endif
    node<DATA_TYPE>** growth_address_ptr;
    while(current_prefix_len < prefix_len - 1){ // 枝を伸ばす
        if(check_bit(prefix, current_prefix_len)){
            growth_address_ptr = &current->node_1;
        }else{
            growth_address_ptr = &current->node_0;
        }
        if((*growth_address_ptr) != nullptr){
            current = *growth_address_ptr;
        }else{
            auto* growth_node = (node<DATA_TYPE>*) malloc(sizeof(node<DATA_TYPE>));
            if(growth_node == nullptr){
                printf("Failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
            growth_node->is_prefix = false;
            growth_node->prefix = current->prefix;
            growth_node->prefix_len = current_prefix_len+1;
            growth_node->parent = current;
            growth_node->node_0 = nullptr;
            growth_node->node_1 = nullptr;
            if(check_bit(prefix, current_prefix_len)){
                growth_node->prefix |= (0b01 << (32 - (current_prefix_len + 1)));
                current->node_1 = growth_node;
            }else{
                current->node_0 = growth_node;
            }
#ifdef TEST_RIB_TREE_TEST_TREE
            printf("Create: %s/%d, %p\n", inet_ntoa(in_addr{.s_addr = htonl(growth_node->prefix)}), current_prefix_len+1, growth_node);
#endif
            current = growth_node;
        }
        current_prefix_len++;
    }

    if(check_bit(prefix, prefix_len - 1)){
        growth_address_ptr = &current->node_1;
    }else{
        growth_address_ptr = &current->node_0;
    }
    if((*growth_address_ptr) == nullptr){
        auto* new_prefix = (node<DATA_TYPE>*) malloc(sizeof(node<DATA_TYPE>));
        if(new_prefix == nullptr){
            printf("Failed to allocate memory\n");
            exit(EXIT_FAILURE);
        }
        new_prefix->is_prefix = true;
        new_prefix->prefix = current->prefix;
        if(check_bit(prefix, prefix_len-1)){
            new_prefix->prefix |= (0b01 << (32 - prefix_len));
        }
#ifdef TEST_RIB_TREE_TEST_TREE
        printf("Create Prefix: %s/%d, %p\n", inet_ntoa(in_addr{.s_addr = htonl(new_prefix->prefix)}), prefix_len, new_prefix);
#endif
        new_prefix->prefix_len = prefix_len;
        new_prefix->data = (DATA_TYPE*) malloc(sizeof(DATA_TYPE));
        if(new_prefix->data == nullptr){
            printf("Failed to allocate memory\n");
            exit(EXIT_FAILURE);
        }
        memcpy(new_prefix->data, &data, sizeof(DATA_TYPE));
        new_prefix->parent = current;
        new_prefix->node_0 = nullptr;
        new_prefix->node_1 = nullptr;
        *growth_address_ptr = new_prefix;
        if(is_updated != nullptr){
            *is_updated = false;
        }
    }else{
#ifdef TEST_RIB_TREE_TEST_TREE
        printf("Exist: %s/%d, %p\n", inet_ntoa(in_addr{.s_addr = htonl(prefix)}), prefix_len, (*growth_address_ptr));
#endif
        if((*growth_address_ptr)->is_prefix){
            if(is_updated != nullptr){
                *is_updated = true; // 書き込み先のノードがもともとプレフィックスだったら、それは経路の更新である
            }
        }else{
            (*growth_address_ptr)->is_prefix = true;
            if(is_updated != nullptr){
                *is_updated = false; // 書き込み先のノードがもともとプレフィックスでなかったら、それは経路の追加である
            }
        }
        if((*growth_address_ptr)->data == nullptr){
            (*growth_address_ptr)->data = (DATA_TYPE*) malloc(sizeof(DATA_TYPE));
            if((*growth_address_ptr)->data == nullptr){
                printf("Failed to allocate memory\n");
                exit(EXIT_FAILURE);
            }
        }
        memcpy((*growth_address_ptr)->data, &data, sizeof(DATA_TYPE));
    }

    return *growth_address_ptr;
}

#endif //TEST_RIB_TREE_TREE_H
