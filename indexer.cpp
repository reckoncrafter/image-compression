#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "headers/stb_image.h"
#include "headers/stb_image_write.h"

#include <bitset>
#include <iostream>
#include <list>

using namespace std;

struct RGB{
    uint8_t r, g, b;

    friend ostream& operator<<(ostream& os, const RGB& t){
        os << "R: " << (int)t.r;
        os << " G: " << (int)t.g;
        os << " B: " << (int)t.b;
        return os;
    }
};

struct INDEX_TAG{
    bitset<6> index;
    bitset<2> depth;

    uint8_t resolve(){
        bitset<8> n;
        for(int i = 7; i >= 2; i--){
            n[i] = index[i-2];
        }
        n[1] = depth[1];
        n[0] = depth[0];
    }
};

list<RGB> hash_table[64];

int hash(RGB pixel){
    return (pixel.r + pixel.g + pixel.b) % 64;
}

RGB* process_image(uint8_t* data, int _pixels, int _channels){
    RGB* prcd = new RGB[_pixels];
    int _size = _pixels * _channels;

    int px = 0;
    for(int i = 0; i <= _size; i += 3){
        prcd[px].r = data[i];
        prcd[px].g = data[i+1];
        prcd[px].b = data[i+2];
        px++; 
    }

    return prcd;
}

int main(){
    int width, height, bpp;
    const int channels = 3;
    uint8_t* rgb_image = stbi_load("rck2.png", &width, &height, &bpp, channels);
    
    const int NUM_PIXELS = width * height; // 4096
    const int IMG_SIZE = width * height * channels;

    RGB* proc = process_image(rgb_image, NUM_PIXELS, channels);

    for(int i = 0; i < NUM_PIXELS; i++){
        cout << proc[i] << endl;
    }


}