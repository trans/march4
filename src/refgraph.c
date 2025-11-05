/*
 * March Language - Reference Graph Implementation
 * Compile-time memory management via reference graph analysis
 */

#define _POSIX_C_SOURCE 200809L

#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Initial capacity for graph arrays */
#define INITIAL_NODE_CAPACITY 16
#define INITIAL_INDEX_CAPACITY 64

/* ============================================================================ */
/* Graph Creation and Destruction */
/* ============================================================================ */

ref_graph_t* ref_graph_create(void) {
    ref_graph_t* graph = malloc(sizeof(ref_graph_t));
    if (!graph) return NULL;

    graph->nodes = malloc(INITIAL_NODE_CAPACITY * sizeof(ref_node_t));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    graph->node_count = 0;
    graph->node_capacity = INITIAL_NODE_CAPACITY;
    graph->next_node_id = 1;  /* Start at 1, 0 is NODE_ID_INVALID */

    /* Initialize lookup table */
    graph->node_index = calloc(INITIAL_INDEX_CAPACITY, sizeof(size_t));
    if (!graph->node_index) {
        free(graph->nodes);
        free(graph);
        return NULL;
    }
    graph->node_index_capacity = INITIAL_INDEX_CAPACITY;

    return graph;
}

void ref_graph_free(ref_graph_t* graph) {
    if (!graph) return;

    /* Free all node children arrays */
    for (size_t i = 0; i < graph->node_count; i++) {
        free(graph->nodes[i].children);
    }

    free(graph->nodes);
    free(graph->node_index);
    free(graph);
}

void ref_graph_clear(ref_graph_t* graph) {
    if (!graph) return;

    /* Free all node children arrays */
    for (size_t i = 0; i < graph->node_count; i++) {
        free(graph->nodes[i].children);
    }

    graph->node_count = 0;
    graph->next_node_id = 1;
    memset(graph->node_index, 0, graph->node_index_capacity * sizeof(size_t));
}

/* ============================================================================ */
/* Node Operations */
/* ============================================================================ */

node_id_t ref_graph_alloc_node(ref_graph_t* graph, type_id_t obj_type) {
    if (!graph) return NODE_ID_INVALID;

    /* Allocate new node ID */
    node_id_t new_id = graph->next_node_id++;

    /* Grow node array if needed */
    if (graph->node_count >= graph->node_capacity) {
        size_t new_capacity = graph->node_capacity * 2;
        ref_node_t* new_nodes = realloc(graph->nodes, new_capacity * sizeof(ref_node_t));
        if (!new_nodes) {
            fprintf(stderr, "Error: Failed to grow ref_graph nodes array\n");
            return NODE_ID_INVALID;
        }
        graph->nodes = new_nodes;
        graph->node_capacity = new_capacity;
    }

    /* Grow index table if needed */
    if (new_id >= graph->node_index_capacity) {
        size_t new_capacity = graph->node_index_capacity * 2;
        while (new_id >= new_capacity) {
            new_capacity *= 2;
        }
        size_t* new_index = realloc(graph->node_index, new_capacity * sizeof(size_t));
        if (!new_index) {
            fprintf(stderr, "Error: Failed to grow ref_graph index table\n");
            return NODE_ID_INVALID;
        }
        /* Zero out new entries */
        memset(new_index + graph->node_index_capacity, 0,
               (new_capacity - graph->node_index_capacity) * sizeof(size_t));
        graph->node_index = new_index;
        graph->node_index_capacity = new_capacity;
    }

    /* Create new node */
    ref_node_t node = {
        .node_id = new_id,
        .object_type = obj_type,
        .is_escaped = false,
        .children = NULL,
        .child_count = 0,
        .child_capacity = 0
    };

    /* Add to nodes array */
    size_t idx = graph->node_count++;
    graph->nodes[idx] = node;

    /* Update index mapping */
    graph->node_index[new_id] = idx;

    return new_id;
}

ref_node_t* ref_graph_get_node(ref_graph_t* graph, node_id_t node_id) {
    if (!graph || node_id == NODE_ID_INVALID || node_id >= graph->node_index_capacity) {
        return NULL;
    }

    size_t idx = graph->node_index[node_id];
    if (idx >= graph->node_count) {
        return NULL;
    }

    return &graph->nodes[idx];
}

void ref_graph_add_child(ref_graph_t* graph, node_id_t parent_id, node_id_t child_id) {
    if (!graph) return;

    ref_node_t* parent = ref_graph_get_node(graph, parent_id);
    if (!parent) {
        fprintf(stderr, "Error: Invalid parent node ID %u\n", parent_id);
        return;
    }

    /* Grow children array if needed */
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        node_id_t* new_children = realloc(parent->children, new_capacity * sizeof(node_id_t));
        if (!new_children) {
            fprintf(stderr, "Error: Failed to grow node children array\n");
            return;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    /* Add child */
    parent->children[parent->child_count++] = child_id;
}

void ref_graph_mark_escaped(ref_graph_t* graph, node_id_t node_id) {
    if (!graph) return;

    ref_node_t* node = ref_graph_get_node(graph, node_id);
    if (node) {
        node->is_escaped = true;
    }
}
