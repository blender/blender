/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

/*
 * This is the original demo application from the PCG library ported to the new API
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include "pcg32.h"

int main(int argc, char** argv) {
    // Read command-line options
    int rounds = 5;

    if (argc > 1)
        rounds = atoi(argv[1]);

    pcg32 rng;

    // You should *always* seed the RNG.  The usual time to do it is the
    // point in time when you create RNG (typically at the beginning of the
    // program).
    //
    // pcg32::seed takes two 64-bit constants (the initial state, and the
    // rng sequence selector; rngs with different sequence selectors will
    // *never* have random sequences that coincide, at all)
    rng.seed(42u, 54u);

    printf("pcg32_random_r:\n"
           "      -  result:      32-bit unsigned int (uint32_t)\n"
           "      -  period:      2^64   (* 2^63 streams)\n"
           "      -  state type:  pcg32_random_t (%zu bytes)\n"
           "      -  output func: XSH-RR\n"
           "\n",
           sizeof(pcg32));

    for (int round = 1; round <= rounds; ++round) {
        printf("Round %d:\n", round);
        /* Make some 32-bit numbers */
        printf("  32bit:");
        for (int i = 0; i < 6; ++i)
            printf(" 0x%08x", rng.nextUInt());
        printf("\n");

        /* Toss some coins */
        printf("  Coins: ");
        for (int i = 0; i < 65; ++i)
            printf("%c", rng.nextUInt(2) ? 'H' : 'T');
        printf("\n");

        /* Roll some dice */
        printf("  Rolls:");
        for (int i = 0; i < 33; ++i) {
            printf(" %d", (int)rng.nextUInt(6) + 1);
        }
        printf("\n");

        /* Deal some cards */
        enum { SUITS = 4, NUMBERS = 13, CARDS = 52 };
        char cards[CARDS];

        for (int i = 0; i < CARDS; ++i)
            cards[i] = i;

        rng.shuffle(cards, cards + CARDS);

        printf("  Cards:");
        static const char number[] = {'A', '2', '3', '4', '5', '6', '7',
                                      '8', '9', 'T', 'J', 'Q', 'K'};
        static const char suit[] = {'h', 'c', 'd', 's'};
        for (int i = 0; i < CARDS; ++i) {
            printf(" %c%c", number[cards[i] / SUITS], suit[cards[i] % SUITS]);
            if ((i + 1) % 22 == 0)
                printf("\n\t");
        }
        printf("\n");

        printf("\n");
    }

    return 0;
}
