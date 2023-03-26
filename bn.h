#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#ifndef SWAP
#define SWAP(x, y)           \
    do {                     \
        typeof(x) __tmp = x; \
        x = y;               \
        y = __tmp;           \
    } while (0)
#endif

#ifndef DIV_ROUNDUP
#define DIV_ROUNDUP(x, len) (((x) + (len) -1) / (len))
#endif

/* digits[size - 1] = msb, digits[0] = lsb */
typedef struct _bn {
    unsigned int *digits;
    unsigned int size;
    int sign;
} bn;

/* count leading zeros of insert*/
static int bn_clz(const bn *insert)
{
    int count = 0;
    for (int i = insert->size - 1; i >= 0; i--) {
        if (insert->digits[i]) {
            count += __builtin_clz(insert->digits[i]);
            return count;
        } else {
            count += 32;
        }
    }
    return count;
}

/*
 * alloc a bn structure with the given size
 * the value is initialized to +0
 */
bn *bn_alloc(size_t size)
{
    bn *new = (bn *) kmalloc(sizeof(bn), GFP_KERNEL);
    new->digits = kmalloc(sizeof(int) * size, GFP_KERNEL);
    memset(new->digits, 0, sizeof(int) * size);
    new->size = size;
    new->sign = 0;
    return new;
}

/*
 * free entire bn data structure
 * return 0 on success, -1 on error
 */
int bn_free(bn *insert)
{
    if (insert == NULL)
        return -1;
    kfree(insert->digits);
    kfree(insert);
    return 0;
}


static int bn_resize(bn *insert, size_t size)
{
    if (!insert)//if insert is not a ptr 
        return -1;
    if (size == insert->size)
        return 0;
    if (size == 0)
        return bn_free(insert);
    insert->digits = krealloc(insert->digits, sizeof(int) * size, GFP_KERNEL);
    if (!insert->digits) {  
        return -1;
    }
    if (size > insert->size)
        memset(insert->digits + insert->size, 0, sizeof(int) * (size - insert->size));
    insert->size = size;
    return 0;
}

/*
 * copy the value from insert to dest
 * return 0 on success, -1 on error
 */
int bn_copy(bn *dest, bn *insert)
{
    if (bn_resize(dest, insert->size) < 0)
        return -1;
    dest->sign = insert->sign;
    memcpy(dest->digits, insert->digits, insert->size * sizeof(int));
    return 0;
}

/*
 * output bn to decimal string
 * Note: the returned string should be freed with kfree()
 */
char *bn_to_string(const bn insert)
{
    // log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322
    size_t len = (8 * sizeof(int) * insert.size) / 3 + 2 + insert.sign;
    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = insert.size - 1; i >= 0; i--) {
        for (unsigned int d = 1U << 31; d; d >>= 1) {
            /* binary -> decimal string */
            int carry = !!(d & insert.digits[i]);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;  // double it
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    // skip leading zero
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }
    if (insert.sign)
        *(--p) = '-';
    memmove(s, p, strlen(p) + 1);
    return s;
}

/*
 * compare length
 * return 1 if |a| > |b|
 * return -1 if |a| < |b|
 * return 0 if |a| = |b|
 */
int bn_cmp(const bn *a, const bn *b)
{
    if (a->size > b->size) {
        return 1;
    } else if (a->size < b->size) {
        return -1;
    } else {
        for (int i = a->size - 1; i >= 0; i--) {
            if (a->digits[i] > b->digits[i])
                return 1;
            if (a->digits[i] < b->digits[i])
                return -1;
        }
        return 0;
    }
}

/* count the digits of most significant bit */
static int bn_msb(const bn *insert)
{
    return insert->size * 32 - bn_clz(insert);
}

/* |c| = |a| + |b| */
static void bn_do_add(const bn *a, const bn *b, bn *c)
{
    // max digits = max(sizeof(a) + sizeof(b)) + 1
    int d = MAX(bn_msb(a), bn_msb(b)) + 1;
    d = DIV_ROUNDUP(d, 32);
    bn_resize(c, d);  // round up, min size = 1

    unsigned long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->digits[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->digits[i] : 0;
        carry += (unsigned long long int) tmp1 + tmp2;
        c->digits[i] = carry;
        carry >>= 32;
    }

    if (!c->digits[c->size - 1] && c->size > 1)
        bn_resize(c, c->size - 1);
}

/*
 * |c| = |a| - |b|
 * Note: |a| > |b| must be true
 */
static void bn_do_sub(const bn *a, const bn *b, bn *c)
{
    // max digits = max(sizeof(a) + sizeof(b))
    int d = MAX(a->size, b->size);
    bn_resize(c, d);

    long long int carry = 0;
    for (int i = 0; i < c->size; i++) {
        unsigned int tmp1 = (i < a->size) ? a->digits[i] : 0;
        unsigned int tmp2 = (i < b->size) ? b->digits[i] : 0;
        carry = (long long int) tmp1 - tmp2 - carry;
        if (carry < 0) {
            c->digits[i] = carry + (1LL << 32);
            carry = 1;
        } else {
            c->digits[i] = carry;
            carry = 0;
        }
    }

    d = bn_clz(c) / 32;
    if (d == c->size)
        --d;
    bn_resize(c, c->size - d);
}

/* c = a + b
 * Note: work for c == a or c == b
 */
void bn_add(const bn *a, const bn *b, bn *c)
{
    if (a->sign == b->sign) {  // both positive or negative
        bn_do_add(a, b, c);
        c->sign = a->sign;
    } else {          // different sign
        if (a->sign)  // let a > 0, b < 0
            SWAP(a, b);
        int cmp = bn_cmp(a, b);
        if (cmp > 0) {
            /* |a| > |b| and b < 0, hence c = a - |b| */
            bn_do_sub(a, b, c);
            c->sign = 0;
        } else if (cmp < 0) {
            /* |a| < |b| and b < 0, hence c = -(|b| - |a|) */
            bn_do_sub(b, a, c);
            c->sign = 1;
        } else {
            /* |a| == |b| */
            bn_resize(c, 1);
            c->digits[0] = 0;
            c->sign = 0;
        }
    }
}

/* c = a - b
 * Note: work for c == a or c == b
 */
void bn_sub(const bn *a, const bn *b, bn *c)
{
    /* xor the sign bit of b and let bn_add handle it */
    bn tmp = *b;
    tmp.sign ^= 1;  // a - b = a + (-b)
    bn_add(a, &tmp, c);
}

/* c += x, starting from offset */
static void bn_mult_add(bn *c, int offset, unsigned long long int x)
{
    unsigned long long int carry = 0;
    for (int i = offset; i < c->size; i++) {
        carry += c->digits[i] + (x & 0xFFFFFFFF);
        c->digits[i] = carry;
        carry >>= 32;
        x >>= 32;
        if (!x && !carry)  // done
            return;
    }
}

void bn_swap(bn *a, bn *b)
{
    bn temp = *a;
    *a = *b;
    *b = temp;
}

/*
 * c = a x b
 * Note: work for c == a or c == b
 * using the simple quadratic-time algorithm (long multiplication)
 */
void bn_mult(const bn *a, const bn *b, bn *c)
{
    // max digits = sizeof(a) + sizeof(b))
    int d = bn_msb(a) + bn_msb(b);
    d = DIV_ROUNDUP(d, 32) + !d;  // round up, min size = 1
    bn *tmp;
    /* make it work properly when c == a or c == b */
    if (c == a || c == b) {
        tmp = c;  // save c
        c = bn_alloc(d);
    } else {
        tmp = NULL;
        for (int i = 0; i < c->size; i++)
            c->digits[i] = 0;
        bn_resize(c, d);
    }

    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            unsigned long long int carry = 0;
            carry = (unsigned long long int) a->digits[i] * b->digits[j];
            bn_mult_add(c, i + j, carry);
        }
    }
    c->sign = a->sign ^ b->sign;

    if (tmp) {
        bn_swap(tmp, c);  // restore c
        bn_free(c);
    }
}

/* left bit shift on bn (maximun shift 31) */
void bn_lshift(const bn *insert, size_t shift, bn *dest)
{
    size_t z = bn_clz(insert);
    shift %= 32;  // only handle shift within 32 bits atm
    if (!shift)
        return;

    if (shift > z) {
        bn_resize(dest, insert->size + 1);
    } else {
        bn_resize(dest, insert->size);
    }
    /* bit shift */
    for (int i = insert->size - 1; i > 0; i--)
        dest->digits[i] =
            insert->digits[i] << shift | insert->digits[i - 1] >> (32 - shift);
    dest->digits[0] = insert->digits[0] << shift;
}