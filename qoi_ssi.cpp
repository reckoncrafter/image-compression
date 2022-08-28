#include <iostream>
#include <vector>
#include <string>
#include <bitset>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../headers/stb_image.h"
#include "../headers/stb_image_write.h"

#define SSI_RUN_LENGTH_TAG 0b01000000

using namespace std;

struct Pixel{
    uint8_t r, g, b;

    friend bool operator==(const Pixel &lhs, const Pixel &rhs){
        return (lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b);
    }
};

enum Type{
    RGB_TAG,
    RUN_LENGTH_TAG,
    INDEX_TAG,
    DIFF_TAG,
};

class Tag{
    struct Triple{
        int r,g,b;

        Triple(){r=0,g=0,b=0;};
        Triple(int _r, int _g, int _b){
            r = _r; g = _g; b = _b;
        }
        Triple(Pixel _p){
            r = _p.r;
            g = _p.g;
            b = _p.b;
        }
        explicit operator Pixel() const {
            Pixel p = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
            return p;
        }
    };

    public:

    Type type;
    Triple rgb;
    int val;

    Tag();
    Tag(Type _t, int _v){
        type = _t;
        val = _v;
    }
    Tag(Type _t, Triple _rgb){
        type = _t;
        rgb = _rgb;
    }

    uint8_t* resolve(){
        uint8_t* n = nullptr;
        switch(this->type){
            case RGB_TAG:
                // tag bits: 00110011

                n = new uint8_t[4]{0};
                n[0] = 0b00110011;
                n[1] = (uint8_t)rgb.r;
                n[2] = (uint8_t)rgb.g;
                n[3] = (uint8_t)rgb.b;
                return n;
            case RUN_LENGTH_TAG:
                // tag bits: 10------
                // 6 bits (64) to encode run-length

                n = new uint8_t(0);
                *n = (uint8_t)0b10000000;
                *n = *n | (val & 0b00111111);
                // vv vvvvvv &
                // 00 111111
                // ---------
                // 00 vvvvvv free up tag bits
                // 10 000000 |
                // ---------
                // 10 vvvvvv write "10" tag bits
                return n;
            case INDEX_TAG:
                // tag bits: 01------
                // 6 bits (64) to encode index

                n = new uint8_t(0);
                *n = (uint8_t)0b01000000;
                *n = *n | (val & 0b00111111);
                return n;
            case DIFF_TAG:
                // tag_bits: 1110----
                // 4 bits (16) to encode each channel's difference

                n = new uint8_t[2]{0};
                n[0] = 0b11100000;
                n[0] = n[0] | (rgb.r & 0b00001111);
                n[1] = 0b00000000;
                n[1] = n[1] | ( ((uint8_t)rgb.g << 4) & 0b11110000);
                n[1] = n[1] | ( (uint8_t)rgb.b & 0b00001111);
                // 1110 0000 0000 0000
                // 0000 rrrr 0000 0000
                // ---------
                // 1100 rrrr 0000 0000
                //    shift: gggg 0000 |
                //           ---------
                // 1100 rrrr gggg 0000
                //           0000 bbbb |
                //           ---------
                // 1100 rrrr gggg bbbb
                return n;
            default:
                return new uint8_t(0);
                break;
        }
    }

    friend ostream & operator << (ostream &out, const Tag &t);

    // RGB_TAG
    // [TAG_BYTE][RED][GREEN][BLUE]
    
    // RUN_LENGTH_TAG
    // [tag1][tag0][b5][b4][b3][b2][b1][b0]

    // INDEX_TAG
    // [tag1][tag0][b5][b4][b3][b2][b1][b0]

    // DIFF_TAG
    // [tag3][tag2][tag1][tag0][dR3][dR2][dR1][dR0] [dB3][dB2][dB1][dB0][db3][db2][db1][db0]
};

ostream & operator << (ostream &out, const Tag &t)
{
    switch(t.type){
        case RGB_TAG:
            out << "RGB_TAG";
            break;
        case RUN_LENGTH_TAG:
            out << "RUN_LENGTH_TAG";
            break;
        case INDEX_TAG:
            out << "INDEX_TAG";
            break;
        case DIFF_TAG:
            out << "DIFF_TAG";
            break;
        default:
            out << "TAG";
    }
    if(t.type == RGB_TAG){
        out << endl;
        out << "R: " << t.rgb.r << endl;
        out << "G: " << t.rgb.g << endl;
        out << "B: " << t.rgb.b;
    }
    else if(t.type == DIFF_TAG){
        out << endl;
        out << "dR: " << t.rgb.r << endl;
        out << "dG: " << t.rgb.g << endl;
        out << "dB: " << t.rgb.b;
    }
    else{
        out << ": " << (int)t.val;
    }
    return out;
}

enum CHANNEL{
    red = 0,
    green = 1,
    blue = 2
};

uint8_t* extract_channel(uint8_t* data, int _size, CHANNEL init){
    uint8_t* ex_channel = new uint8_t[_size];
    vector<uint8_t> extracted;

    for(int i = init; i < _size; i += 3){
        extracted.push_back(data[i]);
    }

    int r_index = 0;
    for(int i = 0; i < _size; i += 3){
        //cout << '[' << channel_red.at(i) << ']';
        //if(i % 64 == 0){cout << endl;}
        ex_channel[i  ] = 0;
        ex_channel[i+1] = 0;
        ex_channel[i+2] = 0;

        switch(init){
            case red:
                ex_channel[i] = extracted.at(r_index);
                break;
            case green:
                ex_channel[i+1] = extracted.at(r_index);
                break;
            case blue:
                ex_channel[i+2] = extracted.at(r_index);
                break;
        }

        r_index++;
    }
    return ex_channel;
}

Pixel* process_image(uint8_t* data, int _pixels, int _channels){
    Pixel* prcd = new Pixel[_pixels];
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

vector<Tag> compress(Pixel* input, int _pixels){
    vector<Tag> result;

    int i = 0;
    uint8_t cnt = 0;

    Tag init_tag = {RGB_TAG, Pixel({input[i].r, input[i].g, input[i].b})};
    result.push_back(init_tag);

    // run-length encoding
    while(i < _pixels){
        if(input[i] == input[i+1] && cnt < 63){
            cnt++;
            i++;
        }
        else{
            if(cnt > 1){
                result.push_back({RUN_LENGTH_TAG, cnt});
            }
            i++;
            Tag _n = {RGB_TAG, Pixel({input[i].r, input[i].g, input[i].b})};
            result.push_back(_n);
            cnt = 0;
        }
    }

    // index encoding
    // hs: generates index position based on pixel values

    
    auto hs = [](Pixel p){return ((p.r+p.g+p.b) % 64);};
    Pixel* table[64] = {nullptr};
    table[hs({0,0,0})] = new Pixel({0,0,0});
    table[hs({255,255,255})] = new Pixel({255,255,255});

    for(Tag &a : result){
        if(a.type == RGB_TAG){
            if(table[hs((Pixel)a.rgb)] != nullptr){
                a = Tag(INDEX_TAG, hs((Pixel)a.rgb));
            }
        }
    }

    // differece encoding
    /*
    const int up = 16;
    const int low = -16;

    Tag* ante = nullptr;
    for(Tag &a : result){
        if(a.type == RGB_TAG && ante != nullptr){
            int d_r = a.rgb.r - (*ante).rgb.r;
            int d_g = a.rgb.g - (*ante).rgb.g;
            int d_b = a.rgb.b - (*ante).rgb.b;
            bool valid = (d_r < up && d_r > low) && (d_g < up && d_g > low) && (d_b < up && d_b > low);
            if(valid){
                a = Tag(DIFF_TAG, {d_r, d_g, d_b});
            }
        }
        ante = &a;
    }
    */

    return result;
}


int main(){
    int width, height, bpp;
    const int channels = 3;
    uint8_t* rgb_image = stbi_load("rck2.png", &width, &height, &bpp, channels);
    
    const int NUM_PIXELS = width * height; // 4096
    const int IMG_SIZE = width * height * channels;

    Pixel* proc = process_image(rgb_image, NUM_PIXELS, channels);
    
    /*
    for(int i = 0; i < NUM_PIXELS; i++){
        cout << "px" << i << ": ";
        cout << "r:" << (int)proc[i].r << " ";
        cout << "g:" << (int)proc[i].g << " ";
        cout << "b:" << (int)proc[i].b << " ";
        cout << endl;
    }
    */
    

    vector<Tag> cmp = compress(proc, NUM_PIXELS);

    
    for(auto a : cmp){
        cout << a << endl;
        uint8_t* b = a.resolve();
        bitset<8> x(b[0]);
        cout << x << endl;
        cout << endl;
    }

    /*
    vector<uint8_t*> bytes;
    for(auto a : cmp){
        bytes.push_back(a.resolve());
    }
    for(auto b : bytes){



    }
    */

    stbi_image_free(rgb_image);
    return 0;
}

