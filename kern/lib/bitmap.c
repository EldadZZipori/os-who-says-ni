/*
 * Copyright (c) 2000, 2001, 2002
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Fixed-size array of bits. (Intended for storage management.)
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <bitmap.h>
#include <vm.h>

/*
 * It would be a lot more efficient on most platforms to use uint32_t
 * or unsigned long as the base type for holding bits. But we don't,
 * because if one uses any data type more than a single byte wide,
 * bitmap data saved on disk becomes endian-dependent, which is a
 * severe nuisance.
 */
#define BITS_PER_WORD   (CHAR_BIT)
#define WORD_TYPE       unsigned char
#define WORD_ALLBITS    (0xff)

struct bitmap {
        unsigned nbits;
        WORD_TYPE *v;
};


struct bitmap *
bitmap_create(unsigned nbits)
{
        struct bitmap *b;
        unsigned words;

        words = DIVROUNDUP(nbits, BITS_PER_WORD);
        b = kmalloc(sizeof(struct bitmap));
        if (b == NULL) {
                return NULL;
        }
        b->v = kmalloc(words*sizeof(WORD_TYPE));
        if (b->v == NULL) {
                kfree(b);
                return NULL;
        }

        bzero(b->v, words*sizeof(WORD_TYPE));
        b->nbits = nbits;

        /* Mark any leftover bits at the end in use */
        if (words > nbits / BITS_PER_WORD) {
                unsigned j, ix = words-1;
                unsigned overbits = nbits - ix*BITS_PER_WORD;

                KASSERT(nbits / BITS_PER_WORD == words-1);
                KASSERT(overbits > 0 && overbits < BITS_PER_WORD);

                for (j=overbits; j<BITS_PER_WORD; j++) {
                        b->v[ix] |= ((WORD_TYPE)1 << j);
                }
        }

        return b;
}

/**
 * @param start the start of the region that we will track. This must 
 *              not include the stolen region for the bitmap itself.
 * @param end   the end of the region that we will track. 
 */
int
bitmap_bootstrap(paddr_t bitmap_address, unsigned nbits)
{
        struct bitmap *b;
        unsigned words;

        words = DIVROUNDUP(nbits, BITS_PER_WORD);
        b = (struct bitmap *) PADDR_TO_KSEG0_VADDR(bitmap_address);//kmalloc(sizeof(struct bitmap));
        bitmap_address += sizeof(struct bitmap);

        b->v = (void *) PADDR_TO_KSEG0_VADDR(bitmap_address);//kmalloc(words*sizeof(WORD_TYPE));
        bitmap_address+= words*sizeof(WORD_TYPE);


        bzero(b->v, words*sizeof(WORD_TYPE));
        b->nbits = nbits;

        /* Mark any leftover bits at the end in use */
        if (words > nbits / BITS_PER_WORD) {
                unsigned j, ix = words-1;
                unsigned overbits = nbits - ix*BITS_PER_WORD;

                KASSERT(nbits / BITS_PER_WORD == words-1);
                KASSERT(overbits > 0 && overbits < BITS_PER_WORD);

                for (j=overbits; j<BITS_PER_WORD; j++) {
                        b->v[ix] |= ((WORD_TYPE)1 << j);
                }
        }

        return bitmap_address;
}

void *
bitmap_getdata(struct bitmap *b)
{
        return b->v;
}

int
bitmap_alloc(struct bitmap *b, unsigned *index)
{
        unsigned ix;
        unsigned maxix = DIVROUNDUP(b->nbits, BITS_PER_WORD);
        unsigned offset;

        for (ix=0; ix<maxix; ix++) {
                if (b->v[ix]!=WORD_ALLBITS) {
                        for (offset = 0; offset < BITS_PER_WORD; offset++) {
                                WORD_TYPE mask = ((WORD_TYPE)1) << offset;

                                if ((b->v[ix] & mask)==0) {
                                        b->v[ix] |= mask;
                                        *index = (ix*BITS_PER_WORD)+offset;
                                        KASSERT(*index < b->nbits);
                                        return 0;
                                }
                        }
                        KASSERT(0);
                }
        }
        return ENOSPC;
}

// int
// bitmap_alloc_nbits(struct bitmap *alloc_bm, struct bitmap *last_page_bm , size_t sz, unsigned *idx)
// {
//         int valid;

//         for (unsigned int i = 0; i < alloc_bm->nbits - sz; i++)
//         {
//                 valid = 1; // assume is valid

//                 for (unsigned int j = 0; j < sz; j++)
//                 {
//                         if (bitmap_isset(alloc_bm, i+j))
//                         {
//                                 valid = 0; // this bit is not valid, so chunk is not valid
//                         }
//                 }
//                 if (valid)
//                 {
//                         *idx = i;
//                         for (unsigned int k = 0; k < sz; k++ )
//                         {
//                                 if(k == sz-1) bitmap_mark(last_page_bm, i+k);
                                
//                                 bitmap_mark(alloc_bm, i+k); // set all these bits to 1
//                                 dumbervm.n_ppages_allocated++;

//                         }
//                         return 0;
//                 }
//         }
//         return -1; // exited loop
// }
int
bitmap_alloc_nbits(struct bitmap *alloc_bm, struct bitmap *last_page_bm, size_t sz, unsigned *idx)
{
    int found = 0;
    unsigned best_i = 0;
    int max_continuous_zeros = -1;

    for (unsigned int i = 0; i <= alloc_bm->nbits - sz; i++)
    {
        int valid = 1;

        for (unsigned int j = 0; j < sz; j++)
        {
            if (bitmap_isset(alloc_bm, i + j))
            {
                valid = 0;
                break;
            }
        }

        if (valid)
        {
            // Simulate allocation to find the maximum continuous zeros after allocation
            int current_max_zeros = 0;
            int temp_zeros = 0;
            for (unsigned int k = 0; k < alloc_bm->nbits; k++)
            {
                int bit = (k >= i && k < i + sz) ? 1 : bitmap_isset(alloc_bm, k);

                if (!bit)
                {
                    temp_zeros++;
                    if (temp_zeros > current_max_zeros)
                        current_max_zeros = temp_zeros;
                }
                else
                {
                    temp_zeros = 0;
                }
            }

            if (current_max_zeros > max_continuous_zeros)
            {
                max_continuous_zeros = current_max_zeros;
                best_i = i;
                found = 1;
            }
        }
    }

    if (found)
    {
        *idx = best_i;
        for (unsigned int k = 0; k < sz; k++)
        {
            if(k == sz-1) bitmap_mark(last_page_bm, best_i+k);
            bitmap_mark(alloc_bm, best_i + k);
            // Additional operations if needed
        }
        return 0;
    }
    return -1; // No suitable block found
}


static
inline
void
bitmap_translate(unsigned bitno, unsigned *ix, WORD_TYPE *mask)
{
        unsigned offset;
        *ix = bitno / BITS_PER_WORD;
        offset = bitno % BITS_PER_WORD;
        *mask = ((WORD_TYPE)1) << offset;
}

void
bitmap_mark(struct bitmap *b, unsigned index)
{
        unsigned ix;
        WORD_TYPE mask;

        KASSERT(index < b->nbits);
        bitmap_translate(index, &ix, &mask);

        KASSERT((b->v[ix] & mask)==0);
        b->v[ix] |= mask;
}

void
bitmap_unmark(struct bitmap *b, unsigned index)
{
        unsigned ix;
        WORD_TYPE mask;

        KASSERT(index < b->nbits);
        bitmap_translate(index, &ix, &mask);

        KASSERT((b->v[ix] & mask)!=0);
        b->v[ix] &= ~mask;
}


int
bitmap_isset(struct bitmap *b, unsigned index)
{
        unsigned ix;
        WORD_TYPE mask;

        bitmap_translate(index, &ix, &mask);
        return (b->v[ix] & mask);
}

void
bitmap_destroy(struct bitmap *b)
{
        kfree(b->v);
        kfree(b);
}
