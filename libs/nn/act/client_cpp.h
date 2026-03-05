namespace nn {
    typedef bool Result;
};

namespace nn::act {
    inline nn::Result GetDeviceHash(char outHash[6]) { return outHash[0] == 0; }
}; // namespace nn::act
