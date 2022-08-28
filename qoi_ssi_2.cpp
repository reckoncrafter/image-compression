#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../headers/stb_image.h"
#include "../headers/stb_image_write.h"

#include <bitset>
#include <iostream>
#include <vector>

using namespace std;

// Note to developer: Bitsets index backwards
// ... you fucking rart

struct triplet{
    uint8_t r, g, b;

    friend bool operator==(const triplet &lhs, const triplet &rhs){
        return (lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b);
    }

    friend triplet operator-(const triplet &lhs, const triplet &rhs){
        triplet t;
        t.r = lhs.r - rhs.r;
        t.g = lhs.g - rhs.g;
        t.b = lhs.b - lhs.b;
        return t;
    }

    friend ostream& operator<<(ostream& os, const triplet& t){
        os << "R: " << (int)t.r;
        os << " G: " << (int)t.g;
        os << " B: " << (int)t.b;
        return os;
    }
};

class TAG{
    virtual size_t size(){return 0;}

    public:
    virtual uint8_t* resolve() = 0;
    // resolve as in "resolve down to bit representation"

    virtual void print(){
        uint8_t* n = resolve();
        for(int i = 0; i < size(); i++){
            for(int j = 7; j >= 0; j--){
                cout << ((bitset<8>)n[i])[j] << ' ';
            }
            cout << "   ";
        }
        cout << endl;
    }
    virtual void print_human_readable() = 0;
};

class RGB : public TAG{
    const static uint8_t identifier = 0b00001111;
    size_t size(){return 4;} // 4 bytes

    public:
    triplet rgb;

    uint8_t* resolve(){
        uint8_t* n = new uint8_t[4];
        n[0] = identifier;
        n[1] = rgb.r;
        n[2] = rgb.g;
        n[3] = rgb.b;
        return n;
    }

    void print_human_readable(){
        cout << "RGB: ";
        cout << rgb;
        cout << endl;
    }
};

class RUN_LENGTH : public TAG{
    const static uint8_t identifier = 0b01;
    size_t size(){return 1;} // 1 byte
    
    public:
    uint8_t run = 0;

    // 2 bits for identifier, 6 bits for run-length
    // range should be 1 - 64, there is no need to encode a run length of zero

    uint8_t* resolve(){
        bitset<8> t = 0;
        t[7] = 0; // identifier bit 1
        t[6] = 1; // identifier bit 0

        for(int i = 5; i >= 0; i--){
            t[i] = ((bitset<6>)run)[i];
        }

        uint8_t* n = new uint8_t;
        *n = t.to_ulong();
        return n;
    }

    void print_human_readable(){
        cout << "RUN_LENGTH: " << (int)run;
        cout << endl;
    }
};

class RGB_DIFF_SHORT : public TAG{
    const static uint8_t identifier = 0b11000000;
    size_t size(){return 1;} // 1 byte

    public:
    triplet rgb; // values should range from 1 to 4;

    // subtract 1 from calculated differences
    // the encoding will be 1 less than the original, as to exploit the fact that we aren't going to encode a difference of zero
    // i.e 00 = 1, 01 = 2, 10 = 3, 11 = 4

    // ii dr dg db
    uint8_t* resolve(){
        bitset<8> t = 0;
        t[7] = 1; // identifier bit 7
        t[6] = 1; // identifier bit 6

        t[5] = ((bitset<2>)rgb.r)[1]; // red half-nybble bit 1
        t[4] = ((bitset<2>)rgb.r)[0]; // red half-nybble bit 0

        t[3] = ((bitset<2>)rgb.g)[1]; // green half-nybble bit 1
        t[2] = ((bitset<2>)rgb.g)[0]; // green half-nybble bit 0

        t[1] = ((bitset<2>)rgb.b)[1]; // blue
        t[0] = ((bitset<2>)rgb.b)[0]; // blue

        uint8_t* n = new uint8_t;
        *n = t.to_ulong();
        return n;
    }

    void print_human_readable(){
        cout << "RGB_DIFF_SHORT: ";
        cout << "DR: " << (int)rgb.r;
        cout << " DG: " << (int)rgb.g;
        cout << " DB: " << (int)rgb.b;
        cout << endl;
    }
};

class RGB_DIFF_LONG : public TAG{
    const static uint8_t identifier = 0b0011;
    size_t size(){return 2;} // 2 bytes

    public:
    triplet rgb; // values should range from 1 - 16
    // same scheme as RGB_DIFF_SHORT

    // iiii drrr dggg dbbb
    // this will span two bytes
    uint8_t* resolve(){
        uint8_t* n = new uint8_t[2];
        bitset<16> t;
        
        // Highest 4 bits are identifier
        for(int i = 15; i > (15-4); i--){
            t[i] = ((bitset<4>)identifier)[i-12]; // offset index to range 3-0
        };

        // Next 4 bits for red
        for(int i = 11; i > (11-4); i--){
            t[i] = ((bitset<4>)rgb.r)[i-8];
        }

        // Next 4 bits for green
        for(int i = 7; i > (7-4); i--){
            t[i] = ((bitset<4>)rgb.g)[i-4];
        }

        // Last 4 bits for blue
        for(int i = 3; i >= 0; i--){
            t[i] = ((bitset<4>)rgb.b)[i]; // no offset needed
        }

        unsigned long t_ulong = t.to_ulong();
        // 00000000 00000000 00000000 00000000  00000000 00000000 00000000 00000000
        //                                                        ^ OUR FUCKING BITS ARE ALL THE WAY OVER HERE
        n[0] = t_ulong >> 8; // push the first 8 bits down so we can stuff it into this byte
        n[1] = t_ulong;
        
        return n;
    }

    void print_human_readable(){
        cout << "RGB_DIFF_LONG: ";
        cout << "DR: " << (int)rgb.r;
        cout << " DG: " << (int)rgb.g;
        cout << " DB: " << (int)rgb.b;
        cout << endl;
    }
};

triplet hash_table[64];

class INDEX : public TAG{
    const static uint8_t identifier = 0b10;
    virtual size_t size(){return 1;} // 1 byte

    // 2 bits for identifier, 6 bits for index location
    // 0 - 63
    public:
    uint8_t index;

    void assign(triplet t){
        index = (t.r + t.g + t.b) % 64;
        hash_table[index] = t;
    }

    triplet get_pixel(){
        return hash_table[index];
    }

    uint8_t* resolve(){
        bitset<8> t = 0;
        t[7] = 1; // identifier bit 1
        t[6] = 0; // identifier bit 0

        for(int i = 5; i >= 0; i--){
            t[i] = ((bitset<6>)index)[i];
        }

        uint8_t* n = new uint8_t;
        *n = t.to_ulong();
        return n;
    }

    void print_human_readable(){
        cout << "INDEX: " << (int)index;
        cout << endl;
    }
};

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

triplet* process_image(uint8_t* data, int _pixels, int _channels){
    triplet* prcd = new triplet[_pixels];
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

vector<TAG*> compress(triplet* input, int _pixels){
    vector<TAG*> result;

    int i = 0;
    uint8_t cnt = 0;

    RGB* init_tag = new RGB;
    init_tag->rgb = input[i];
    result.push_back(init_tag);

    // run-length encoding
    while(i < _pixels){
        if(input[i] == input[i+1] && cnt < 63){
            cnt++;
            i++;
        }
        else{
            if(cnt > 1){
                RUN_LENGTH* rn = new RUN_LENGTH;
                rn->run = cnt;
                result.push_back(rn);
            }
            i++;
            RGB* _n = new RGB;
            _n->rgb = input[i];
            result.push_back(_n);

            cnt = 0;
        }
    }
    return result;
}

int main(){
    { // test scope
    cout << "RGB test:" << endl;
    RGB test0;
    test0.rgb.r = 0b01010111;
    test0.rgb.g = 0b11100100;
    test0.rgb.b = 0b11111110;
    
    test0.print();

    cout << "RUN_LENGTH test: " << endl;
    RUN_LENGTH test0a;
    test0a.run = 25;
    test0a.print();

    cout << "RGB_DIFF_SHORT test:" << endl;
    RGB_DIFF_SHORT test1;
    test1.rgb.r = 0b00;
    test1.rgb.g = 0b01;
    test1.rgb.b = 0b11;

    test1.print();

    cout << "RGB_DIFF_LONG test:" << endl;
    RGB_DIFF_LONG test2;
    test2.rgb.r = 0b0111;
    test2.rgb.g = 0b0110;
    test2.rgb.b = 0b1100;

    test2.print();

    cout << "INDEX test:" << endl;
    INDEX test3;
    triplet test3_pixel = {0b01010111, 0b11100100, 0b11111110};
    test3.assign(test3_pixel);
    
    test3.print();
    cout << "index: " << (bitset<6>)test3.index << endl;
    cout << test3.get_pixel() << endl;
    }

    int width, height, bpp;
    const int channels = 3;
    uint8_t* rgb_image = stbi_load("rck2.png", &width, &height, &bpp, channels);
    
    const int NUM_PIXELS = width * height; // 4096
    const int IMG_SIZE = width * height * channels;

    triplet* proc = process_image(rgb_image, NUM_PIXELS, channels);

    vector<TAG*> compressed = compress(proc, NUM_PIXELS);

    for(auto a : compressed){
        a->print_human_readable();
    }

    return 0;
}

