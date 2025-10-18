
#pragma once

#ifndef TREE_H
#define TREE_H

#include <algorithm>
#include <stdint.h>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <queue>
#include <stack>


namespace algorithm {
    // 树节点
    template <typename T>
    struct TreeNode {
        T data;
        TreeNode<T>* parent;
        std::vector<TreeNode<T>*> children;
    };

    // 树
    template <typename T>
    class Tree {
        public:
            // 获取该树的根节点。
            TreeNode<T>* get_root() { return root; }

            // 树的深度优先遍历迭代器。
            class dfs_iterator {
                public:
                    dfs_iterator(TreeNode<T>* root) : current(root) {}
                    TreeNode<T>* operator*() { return current; }
                    dfs_iterator& operator++();
                    dfs_iterator operator++(int);
                    dfs_iterator& operator+=(int n);
                    bool operator!=(const dfs_iterator& other) { return current != other.current || this->end != other.end; }
                    bool operator==(const dfs_iterator& other) { return current == other.current || this->end == other.end; }
                private:
                    TreeNode<T>* current;
                    std::stack<TreeNode<T>*> stack;
                    bool finished;
                    std::vector<size_t> index;
                    void next();
            }

            // 树的广度优先遍历迭代器。
            class bfs_iterator {
                public:
                    bfs_iterator(TreeNode<T>* root) : current(root) {}
                    TreeNode<T>* operator*() { return current; }
                    bfs_iterator& operator++();
                    bfs_iterator operator++(int);
                    bfs_iterator& operator+=(int n);
                    bool operator!=(const bfs_iterator& other) 
                        { return current != other.current || this->end != other.end; }
                    bool operator==(const bfs_iterator& other) 
                        { return current == other.current || this->end == other.end; }

                private:
                    TreeNode<T>* current;
                    std::queue<TreeNode<T>*> queue;
                    int depth;
                    bool finished;
                    std::vector<size_t> index;
                    void next();
            }

            // 设置该树的根节点。
            void set_root(TreeNode<T>* root) {
                this->root = root;
            }

            // 获取一个结点的深度。
            int get_depth(TreeNode<T>* node);
            // 获取该树的高度。
            int get_height();
            // 获取该树的叶子节点数量。
            int get_leaf_count();
            // 获取该树的节点数量。
            int get_node_count();
            // 获取该节点的父节点。
            TreeNode<T>* get_parent(TreeNode<T>* node);
            // 获取该节点的所有兄弟节点。（不包括自己）
            std::vector<TreeNode<T>*> get_siblings(TreeNode<T>* node);
            // 获取该节点的所有祖先节点。（不包括自己）
            std::vector<TreeNode<T>*> get_ancestors(TreeNode<T>* node);
            // 获取该节点的所有后代节点。（不包括自己）
            std::vector<TreeNode<T>*> get_descendants(TreeNode<T>* node);

            // 获取该树的深度优先遍历迭代器。
            dfs_iterator dbegin();
            // 获取该树的深度优先遍历迭代器。
            dfs_iterator dend();
            // 获取该树的广度优先遍历迭代器。
            bfs_iterator bbegin();
            // 获取该树的广度优先遍历迭代器。
            bfs_iterator bend();
            

        private:
            TreeNode<T>* root;

    };


};

#endif // TREE_H