/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * avl.c
 */

#include "scm.h"
#include "avl.h"

struct avl
{
    struct state
    {
        uint64_t items;
        uint64_t unique;
        struct node
        {
            int depth;
            uint64_t count;
            const char *item;
            struct node *left;
            struct node *right;
        } *root;
    } *state; /* SCM */
    struct scm *scm;
};

static int
delta(const struct node *node)
{
    return node ? node->depth : -1;
}

static int
balance(const struct node *node)
{
    return delta(node->left) - delta(node->right);
}

static int
depth(const struct node *a, const struct node *b)
{
    return (delta(a) > delta(b)) ? (delta(a) + 1) : (delta(b) + 1);
}

static struct node *
rotate_right(struct node *node)
{
    struct node *root;

    root = node->left;
    node->left = root->right;
    root->right = node;
    node->depth = depth(node->left, node->right);
    root->depth = depth(root->left, node);
    return root;
}

static struct node *
rotate_left(struct node *node)
{
    struct node *root;

    root = node->right;
    node->right = root->left;
    root->left = node;
    node->depth = depth(node->left, node->right);
    root->depth = depth(root->right, node);
    return root;
}

static struct node *
rotate_left_right(struct node *node)
{
    node->left = rotate_left(node->left);
    return rotate_right(node);
}

static struct node *
rotate_right_left(struct node *node)
{
    node->right = rotate_right(node->right);
    return rotate_left(node);
}

static struct node *
update(struct avl *avl, struct node *root, const char *item)
{
    int d;

    if (!root) /* if root is NULL */
    {
        if (!(root = scm_malloc(avl->scm, sizeof(struct node)))) /* allocate memory for word */
        {
            TRACE(0);
            return NULL;
        }
        memset(root, 0, sizeof(struct node));
        if (!(root->item = scm_strdup(avl->scm, item)))
        {
            TRACE(0);
            return NULL;
        }
        printf(" %s: new root constructed in update\n", root->item);
        ++root->count;
        ++avl->state->items;
        ++avl->state->unique;
        return root;
    }
    if (!(d = strcmp(item, root->item))) /* if item already exists */
    {
        printf(" %s already exists in update\n", item);
        ++root->count;
        ++avl->state->items;
    }
    else if (0 > d) /* if item is lower(in ASCII) than root */
    {
        printf(" %s is lower than root %s in update\n", item, root->item);
        root->left = update(avl, root->left, item);
    }
    else if (0 < d) /* if item is higher(in ASCII) than root */
    {
        printf(" %s is higher than root %s in update\n", item, root->item);
        root->right = update(avl, root->right, item);
    }
    root->depth = depth(root->left, root->right);

    if (1 < abs(balance(root)))
    {
        if (0 < strcmp(item, root->right->item))
        {
            root = rotate_left(root);
        }
        else
        {
            root = rotate_right_left(root);
        }
        if (0 > strcmp(item, root->left->item))
        {
            root = rotate_right(root);
        }
        else
        {
            root = rotate_left_right(root);
        }
    }

    return root;
}

static void
traverse(struct node *node, avl_fnc_t fnc, void *arg)
{
    if (node)
    {
        traverse(node->left, fnc, arg);
        fnc(arg, node->item, node->count);
        traverse(node->right, fnc, arg);
    }
}

struct avl *
avl_open(const char *pathname, int truncate)
{
    struct avl *avl;

    assert(pathname);

    if (!(avl = malloc(sizeof(struct avl))))
    {
        TRACE("out of memory");
        return NULL;
    }
    memset(avl, 0, sizeof(struct avl));
    if (!(avl->scm = scm_open(pathname, truncate)))
    {
        avl_close(avl);
        TRACE(0);
        return NULL;
    }
    if (scm_utilized(avl->scm))
    {
        avl->state = scm_mbase(avl->scm);
    }
    else
    {
        if (!(avl->state = scm_malloc(avl->scm,
                                      sizeof(struct state))))
        {
            avl_close(avl);
            TRACE(0);
            return NULL;
        }
        memset(avl->state, 0, sizeof(struct state));
        assert(avl->state == scm_mbase(avl->scm));
    }
    return avl;
}

void avl_close(struct avl *avl)
{
    if (avl)
    {
        scm_close(avl->scm);
        memset(avl, 0, sizeof(struct avl));
    }
    FREE(avl);
}

int avl_insert(struct avl *avl, const char *item)
{
    struct node *root;

    assert(avl);
    assert(safe_strlen(item));

    if (!(root = update(avl, avl->state->root, item)))
    {
        TRACE(0);
        return -1;
    }
    avl->state->root = root;
    printf("root of avl: %s\n", avl->state->root->item);
    return 0;
}

uint64_t
avl_exists(const struct avl *avl, const char *item)
{
    const struct node *node;
    int d;

    assert(avl);
    assert(safe_strlen(item));

    node = avl->state->root;
    while (node)
    {
        if (!(d = strcmp(item, node->item)))
        {
            return node->count;
        }
        node = (0 > d) ? node->left : node->right;
    }
    return 0;
}

/* traverse the tree to get all items and their count */
void avl_traverse(const struct avl *avl, avl_fnc_t fnc, void *arg)
{
    assert(avl);
    assert(fnc);

    traverse(avl->state->root, fnc, arg);
}

uint64_t
avl_items(const struct avl *avl)
{
    assert(avl);

    return avl->state->items;
}

uint64_t
avl_unique(const struct avl *avl)
{
    assert(avl);

    return avl->state->unique;
}

size_t
avl_scm_utilized(const struct avl *avl)
{
    assert(avl);

    return scm_utilized(avl->scm);
}

size_t
avl_scm_capacity(const struct avl *avl)
{
    assert(avl);

    return scm_capacity(avl->scm);
}

static struct node *avl_delete_node(struct avl *avl, struct node *root, const char *item)
{
    int d;

    if (!root)
    {
        return NULL;
    }

    d = strcmp(item, root->item);

    /* keep searching */
    if (d < 0) /* if item is lower(in ASCII) than root */
    {
        root->left = avl_delete_node(avl, root->left, item);
        if (1 < abs(balance(root)))
        {
            if (0 > strcmp(item, root->left->item))
            {
                root = rotate_right(root);
            }
            else
            {
                root = rotate_left_right(root);
            }
        }
    }
    else if (d > 0) /* if item is higher(in ASCII) than root */
    {
        root->right = avl_delete_node(avl, root->right, item);
        if (1 < abs(balance(root)))
        {
            if (0 < strcmp(item, root->right->item))
            {
                root = rotate_left(root);
            }
            else
            {
                root = rotate_right_left(root);
            }
        }
    }
    else /* find */
    {
        if ((root->left == NULL) || (root->right == NULL))
        {
            struct node *temp = root->left ? root->left : root->right;

            if (temp == NULL) /* no child */
            {
                temp = root;
                root = NULL;
            }
            else
            {
                /* Copy the contents of the non-empty child */
                *root = *temp;
            }

            scm_free(avl->scm, (void *)temp->item);
            scm_free(avl->scm, temp);
        }
        else
        {
            struct node *min;

            min = root->right;
            while (min->left)
            {
                min = min->left;
            }
            root->item = min->item;
            root->count = min->count;
            root->right = avl_delete_node(avl, root->right, min->item);
        }
    }

    if (!root)
    {
        return root;
    }

    root->depth = depth(root->left, root->right);

    return root;
}

int avl_delete(struct avl *avl, const char *item)
{
    uint64_t exists;

    assert(avl);
    assert(safe_strlen(item));

    if (!(exists = avl_exists(avl, item)))
    {
        printf("'%s' does not exist\n", item);
        return -1;
    }

    if (!avl->state->root)
    {
        TRACE("item not found");
        return -1;
    }

    avl->state->root = avl_delete_node(avl, avl->state->root, item);

    avl->state->items -= exists;
    avl->state->unique -= 1;

    printf("new avl state items: %lu\n", avl->state->items);
    printf("new avl state unique: %lu\n", avl->state->unique);
    printf("new avl root item: %s\n", avl->state->root->item);

    return 0;
}