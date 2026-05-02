/*---------------------------------------------------------------------------*\

  radae_v2_tx_nopy.c

  RADE V2 transmitter - C port of tx2.py.
  Reads vocoder features from stdin, writes IQ samples to stdout.

  Input:  float32, RADE_V2_FRAMES_PER_STEP * RADE_V2_NB_TOTAL_FEATURES = 144
          floats per modem frame (lpcnet_demo padded format, 36 floats/frame)
  Output: complex float32 (interleaved I,Q), RADE_V2_NMF = 320 samples per
          modem frame, followed by RADE_V2_NEOO = 960 EOO samples.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2025 David Rowe

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  - Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  - Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rade_tx_v2.h"
#include "rade_v2_ofdm.h"

int main(void) {
    rade_tx_v2_state tx;

    if (rade_tx_v2_init(&tx) != 0) {
        fprintf(stderr, "rade_tx_v2_init failed\n");
        return 1;
    }

    int n_features_in = rade_tx_v2_n_features_in();
    int n_tx_out      = rade_tx_v2_n_samples_out();
    int n_eoo_out     = rade_tx_v2_n_eoo_out();

    fprintf(stderr, "n_features_in: %d  n_tx_out: %d  n_eoo_out: %d\n",
            n_features_in, n_tx_out, n_eoo_out);

    float     *features_in = malloc(sizeof(float)     * n_features_in);
    RADE_COMP *tx_out      = malloc(sizeof(RADE_COMP)  * n_tx_out);
    RADE_COMP *eoo_out     = malloc(sizeof(RADE_COMP)  * n_eoo_out);

    if (!features_in || !tx_out || !eoo_out) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    int frame_count = 0;
    while (1) {
        size_t n = fread(features_in, sizeof(float), n_features_in, stdin);
        if (n != (size_t)n_features_in)
            break;
        int n_out = rade_tx_v2_process(&tx, tx_out, features_in);
        fwrite(tx_out, sizeof(RADE_COMP), n_out, stdout);
        frame_count++;
    }

    int n_out = rade_tx_v2_eoo(&tx, eoo_out);
    fwrite(eoo_out, sizeof(RADE_COMP), n_out, stdout);

    fprintf(stderr, "radae_v2_tx_nopy: %d modem frames + EOO\n", frame_count);

    free(features_in);
    free(tx_out);
    free(eoo_out);
    return 0;
}
