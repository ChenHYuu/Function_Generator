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

// 波形生成函數
void generate_wave(float *buffer, int fs, int samples, float f, float A, const char *wavetype) {
    for (int i = 0; i < samples; i++) {
        float t = (float)i / fs;
        if (strcmp(wavetype, "sine") == 0) {
            buffer[i] = A * sin(2 * PI * f * t);
        } else if (strcmp(wavetype, "square") == 0) {
            buffer[i] = A * (sin(2 * PI * f * t) > 0 ? 1 : -1);
        } else if (strcmp(wavetype, "sawtooth") == 0) {
            buffer[i] = A * (2 * (f * t - floor(0.5 + f * t)));
        } else if (strcmp(wavetype, "triangle") == 0) {
            buffer[i] = A * (2 * fabs(2 * (f * t - floor(0.5 + f * t))) - 1);
        } else {
            buffer[i] = 0;
        }
    }
}

// 音訊資料寫入函數
void write_samples_to_wav(int m, float *buffer, int samples, FILE *wav_file) {
    if (m == 8) {
        for (int i = 0; i < samples; i++) {
            // 限制範圍，避免溢出
            float sample = fminf(fmaxf(buffer[i], -1.0), 1.0);
            unsigned char output = (unsigned char)((sample + 1.0) * 127.5); // 8-bit 無符號
            fwrite(&output, sizeof(unsigned char), 1, wav_file);
        }
    } else if (m == 16) {
        for (int i = 0; i < samples; i++) {
            // 限制範圍，避免溢出
            float sample = fminf(fmaxf(buffer[i], -1.0), 1.0);
            short int output = (short int)(sample * 32767); // 16-bit 有符號
            fwrite(&output, sizeof(short int), 1, wav_file);
        }
    } else if (m == 32) {
        for (int i = 0; i < samples; i++) {
            // 限制範圍，避免溢出
            float sample = fminf(fmaxf(buffer[i], -1.0), 1.0);
            int output = (int)(sample * 2147483647); // 32-bit 有符號
            fwrite(&output, sizeof(int), 1, wav_file);
        }
    } else {
        fprintf(stderr, "不支援的位元深度: %d\n", m);
    }
}

float calculate_sqnr(float *signal, int samples, int m) {
    float signal_power = 0.0, noise_power = 0.0;

    for (int i = 0; i < samples; i++) {
        signal_power += signal[i] * signal[i];
    }
    signal_power /= samples;

    float quantization_step = 1.0 / (pow(2, m - 1));
    for (int i = 0; i < samples; i++) {
        float quantized_value = round(signal[i] / quantization_step) * quantization_step;
        float error = quantized_value - signal[i];
        noise_power += error * error;
    }
    noise_power /= samples;

    return 10 * log10(signal_power / noise_power);
}

int main(int argc, char **argv) {
    if (argc != 8) {
        fprintf(stderr, "使用方法: mini_prj_3_xxxxxxxxx fs m c wavetype f A T 1> fn.wav 2> sqnr.txt\n");
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
    float *buffer = (float *)malloc(samples * sizeof(float));
    if (buffer == NULL) {
        fprintf(stderr, "記憶體配置失敗\n");
        return 1;
    }

    generate_wave(buffer, fs, samples, f, A, wavetype);

    WaveHeader header = makeHeader(fs, m, samples, c);

    // 將 WAV 標頭寫入 stdout，將被重定向到 fn.wav
    fwrite(&header, sizeof(WaveHeader), 1, stdout);
    // 將音訊樣本寫入 stdout，將被重定向到 fn.wav
    write_samples_to_wav(m, buffer, samples, stdout);

    // 計算並將 SQNR 寫入 stderr，將被重定向到 sqnr.txt
    float sqnr = calculate_sqnr(buffer, samples, m);
    fprintf(stderr, "SQNR: %.15f dB\n", sqnr);

    free(buffer);
    return 0;
}
