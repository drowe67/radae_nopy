/*---------------------------------------------------------------------------*\

  radae_v2_rx_nopy.c

  RADE V2 receiver - C port of rx2.py.
  Reads IQ samples from stdin, writes vocoder features to stdout.

  Input:  complex float32 (interleaved I,Q), RADE_V2_SYM_LEN = 160 samples
          per call (nin-based; may vary during timing adjustment).
  Output: float32, RADE_V2_FEATURES_OUT = 144 floats per decoded frame
          (RADE_V2_FRAMES_PER_STEP * RADE_V2_NB_TOTAL_FEATURES, 36 floats each,
          padded to lpcnet_demo format).

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

#include "rade_rx_v2.h"

int main(int argc, char *argv[]) {
    int   bpf_en        = 1;   /* BPF enabled by default */
    int   verbose       = 0;
    char *snr_est_fn    = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no_bpf") == 0)  bpf_en = 0;
        if (strcmp(argv[i], "--verbose") == 0)  verbose = 1;
        if (strcmp(argv[i], "--quiet") == 0)    verbose = 0;
        if (strcmp(argv[i], "--write_snr_est") == 0 && i+1 < argc)
            snr_est_fn = argv[++i];
    }

    rade_rx_v2_state rx;
    if (rade_rx_v2_init(&rx, bpf_en) != 0) {
        fprintf(stderr, "rade_rx_v2_init failed\n");
        return 1;
    }
    rx.verbose = verbose;

    int n_features_out = rade_rx_v2_n_features_out();
    int nin_max        = rade_rx_v2_nin_max();

    float     *features_out = malloc(sizeof(float)     * n_features_out);
    RADE_COMP *rx_in        = malloc(sizeof(RADE_COMP)  * nin_max);
    if (!features_out || !rx_in) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    int sym_count   = 0;
    int frame_count = 0;
    int eoo_count   = 0;

    /* Dynamic array for per-symbol SNR log */
    int    snr_log_size = 0;
    int    snr_log_cap  = 0;
    float *snr_log      = NULL;

    while (1) {
        int nin = rade_rx_v2_nin(&rx);

        size_t n = fread(rx_in, sizeof(RADE_COMP), nin, stdin);
        if (n != (size_t)nin)
            break;

        int flags = rade_rx_v2_process(&rx, features_out, rx_in);

        if (flags & 0x1) {
            fwrite(features_out, sizeof(float), n_features_out, stdout);
            frame_count++;
        }
        if (flags & 0x2)
            eoo_count++;

        if (snr_est_fn) {
            if (snr_log_size == snr_log_cap) {
                snr_log_cap = snr_log_cap ? snr_log_cap * 2 : 4096;
                snr_log = realloc(snr_log, sizeof(float) * snr_log_cap);
            }
            snr_log[snr_log_size++] = rx.snr_est_dB;
        }

        sym_count++;
    }

    fprintf(stderr, "radae_v2_rx: symbols: %d  decoded frames: %d  n_acq: %d  eoo: %d\n",
            sym_count, frame_count, rx.n_acq, eoo_count);

    if (snr_est_fn && snr_log) {
        FILE *f = fopen(snr_est_fn, "wb");
        if (f) {
            fwrite(snr_log, sizeof(float), snr_log_size, f);
            fclose(f);
        } else {
            fprintf(stderr, "error: cannot write %s\n", snr_est_fn);
        }
    }

    free(features_out);
    free(rx_in);
    free(snr_log);
    return 0;
}
