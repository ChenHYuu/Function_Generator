#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define PI 3.141592653589793

typedef struct WAVE_HEADER {
    char chunkId[4];       // "RIFF"
    int chunkSize;         // 36 + subChunkSize2
    char fileFormat[4];    // "WAVE"

    char subChunkId1[4];   // "fmt "
    int subChunkSize1;     // 16 for PCM
    short int waveFmt;     // PCM = 1
    short int numChannel;  // Mono = 1, Stereo = 2
    int sampleRate;        // ex. 48000
    int byteRate;          // sampleRate * numChannel * bitsPerSample / 8
    short int blockAlign;  // numChannel * bitsPerSample / 8
    short int bitsPerSample;  // 8 bits = 8, 16 bits = 16

    char subChunkId2[4];   // "data"
    int subChunkSize2;     // Number of bytes of the actual sound data
} WaveHeader;

WaveHeader makeHeader(int sampleRate, int sampleSize, int sampleNum, int numChannel) {
    WaveHeader wh;

    memcpy(wh.chunkId, "RIFF", 4);
    memcpy(wh.fileFormat, "WAVE", 4);
    memcpy(wh.subChunkId1, "fmt ", 4);
    memcpy(wh.subChunkId2, "data", 4);

    wh.subChunkSize1 = 16;
    wh.waveFmt = 1;
    wh.numChannel = numChannel;
    wh.sampleRate = sampleRate;
    wh.bitsPerSample = sampleSize;

    wh.byteRate = sampleRate * numChannel * sampleSize / 8;
    wh.blockAlign = numChannel * sampleSize / 8;
    wh.subChunkSize2 = sampleNum * numChannel * sampleSize / 8;
    wh.chunkSize = 4 + (8 + wh.subChunkSize1) + (8 + wh.subChunkSize2);

    return wh;
}

void generate_wave(float *signal, float *quantized, int fs, int samples, float f, float A, const char *wavetype, int m) {
    float max_amplitude = (m == 8) ? 127.5 : (m == 16) ? 32767.0 : 2147483647.0;

    for (int i = 0; i < samples; i++) {
        float t = (float)i / fs;

        // 生成原始波形
        if (strcmp(wavetype, "sine") == 0) {
            signal[i] = A * sin(2 * PI * f * t);
        } else if (strcmp(wavetype, "square") == 0) {
            signal[i] = A * (sin(2 * PI * f * t) > 0 ? 1 : -1);
        } else if (strcmp(wavetype, "sawtooth") == 0) {
            signal[i] = A * (2 * (f * t - floor(0.5 + f * t)));
        } else if (strcmp(wavetype, "triangle") == 0) {
            signal[i] = A * (2 * fabs(2 * (f * t - floor(0.5 + f * t))) - 1);
        } else {
            signal[i] = 0;
        }

        // 生成量化後的訊號
        quantized[i] = round(signal[i] * max_amplitude) / max_amplitude;
    }
}

void normalize_samples(float *quantized, int samples, int m, FILE *wav_file) {
    for (int i = 0; i < samples; i++) {
        if (m == 8) {
            unsigned char output = (unsigned char)((quantized[i] + 1.0) / 2.0 * 255);
            fwrite(&output, sizeof(unsigned char), 1, wav_file);
        } else if (m == 16) {
            short int output = (short int)(quantized[i] * 32767);
            fwrite(&output, sizeof(short int), 1, wav_file);
        } else if (m == 32) {
            int output = (int)(quantized[i] * 2147483647);
            fwrite(&output, sizeof(int), 1, wav_file);
        } else {
            fprintf(stderr, "[Error] 不支援的 sample size: %d\n", m);
            return;
        }
    }
}

float calculate_sqnr(float *signal, float *quantized, int size) {
    float signal_power = 0.0;
    float noise_power = 0.0;

    for (int i = 0; i < size; i++) {
        signal_power += signal[i] * signal[i];
        noise_power += (signal[i] - quantized[i]) * (signal[i] - quantized[i]);
    }

    signal_power /= size;
    noise_power /= size;

    return 10 * log10(signal_power / noise_power);
}

int main(int argc, char **argv) {
    if (argc != 8) {
        fprintf(stderr, "[Error] 使用方法: mini_prj_3_xxxxxxxxx fs m c wavetype f A T 1> fn.wav 2> sqnr.txt\n");
        return 1;
    }

    int fs = atoi(argv[1]);
    int m = atoi(argv[2]);
    int c = atoi(argv[3]);
    char *wavetype = argv[4];
    float f = atof(argv[5]);
    float A = atof(argv[6]);
    float T = atof(argv[7]);

    int samples = (int)(fs * T);
    float *signal = (float *)malloc(samples * sizeof(float));
    float *quantized = (float *)malloc(samples * sizeof(float));
    if (signal == NULL || quantized == NULL) {
        fprintf(stderr, "[Error] 記憶體配置失敗\n");
        free(signal);
        free(quantized);
        return 1;
    }

    generate_wave(signal, quantized, fs, samples, f, A, wavetype, m);

    WaveHeader header = makeHeader(fs, m, samples, c);
    fwrite(&header, sizeof(WaveHeader), 1, stdout);
    normalize_samples(quantized, samples, m, stdout);

    float sqnr = calculate_sqnr(signal, quantized, samples);
    fprintf(stderr, "SQNR: %.15f dB\n", sqnr);

    free(signal);
    free(quantized);
    return 0;
}
