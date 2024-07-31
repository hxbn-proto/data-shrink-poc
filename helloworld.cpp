#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <random>

using namespace std;

template <typename T>
void println(const T &value)
{
    std::cout << value << std::endl;
}

// Function to generate an array of random bytes
vector<uint8_t> generateRandomBytes(size_t size)
{
    // Create a random device and a random number generator
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<uint8_t> dis(0, 3);

    // Create a vector to hold the random bytes
    vector<uint8_t> randomBytes(size);

    // Fill the vector with random bytes
    for (size_t i = 0; i < size; ++i)
    {
        randomBytes[i] = dis(gen);
    }

    return randomBytes;
}

// Function to return the number of bits required to represent a number
size_t minBits(size_t number)
{
    if (number == 0)
        return 1; // Special case: zero requires at least 1 bit

    size_t bits = 0;
    size_t temp = number;

    // Count the number of bits needed
    while (temp > 0)
    {
        temp >>= 1; // Right shift to divide by 2
        bits++;
    }

    return bits;
}

class VariableBitContainer
{
private:
    size_t maxIndex = 0;
    vector<size_t> indexes;

public:
    size_t getMinBitsToStoreIndex() const
    {
        if (maxIndex == 0)
            return 1; // Special case: zero requires at least 1 bit

        size_t bits = 0;
        size_t temp = maxIndex;

        // Count the number of bits needed
        while (temp > 0)
        {
            temp >>= 1; // Right shift to divide by 2
            bits++;
        }

        return bits;
    }

    // Adds an index and updates maxIndex if necessary
    void addIndex(size_t val)
    {
        if (val > maxIndex)
        {
            maxIndex = val;
        }
        indexes.push_back(val);
    }

    // Returns the minimum number of bits required to store maxIndex
    static size_t minBits(size_t value)
    {
        return value == 0 ? 1 : static_cast<size_t>(log2(value)) + 1;
    }

    // Returns the size in bytes required to store all indexes
    size_t getSizeInBytes() const
    {
        size_t totalBits = getMinBitsToStoreIndex() * indexes.size();
        return (totalBits + 7) / 8; // rounding up to the nearest byte
    }

    /**
     * Serializes the VariableBitContainer object to a vector of bytes.
     *
     * @return The serialized data as a vector of uint8_t.
     */
    vector<uint8_t> serialize() const
    {
        // Calculate the number of bits required to store all indexes
        size_t bitCount = getMinBitsToStoreIndex();
        size_t totalBits = bitCount * indexes.size();
        size_t dataSize = (totalBits + 7) / 8; // rounding up to the nearest byte

        // Allocate the necessary bytes for the serialized data
        vector<uint8_t> data(dataSize + sizeof(maxIndex), 0);

        // Serialize bitCount (first 4 bytes of data)
        for (size_t byteIndex = 0; byteIndex < sizeof(unsigned long); byteIndex++)
        {
            data[byteIndex] = (maxIndex >> (byteIndex * 8)) & 0xFF;
        }

        size_t bitIndex = 0; // Initialize bit index

        for (size_t value : indexes) // Iterate over each value in indexes
        {
            for (size_t bit = 0; bit < bitCount; bit++) // Iterate over each bit position in value
            {
                if (value & (1ULL << bit)) // Check if the bit at position 'bit' is set
                {
                    // Set the corresponding bit in the data array
                    data[sizeof(unsigned long) + bitIndex / bitCount] |= (1 << (bitIndex % bitCount));
                }
                bitIndex++; // Move to the next bit position
            }
        }

        return data;
    }

    // The deserialization method
    void deserialize(const vector<uint8_t> &data)
    {
        if (data.size() < sizeof(size_t))
        {
            throw runtime_error("Data too small for deserialization.");
        }

        // Extract bitCount from the first part of data
        size_t bitCount = 0;
        for (size_t byteIndex = 0; byteIndex < sizeof(size_t); ++byteIndex)
        {
            bitCount |= static_cast<size_t>(data[byteIndex]) << (byteIndex * 8);
        }

        // Calculate the number of indexes
        size_t totalBits = (data.size() - sizeof(size_t)) * 8;
        size_t numIndexes = totalBits / bitCount;

        // Deserialize indexes
        indexes.clear();
        indexes.reserve(numIndexes);
        size_t bitIndex = sizeof(size_t) * 8;
        for (size_t i = 0; i < numIndexes; ++i)
        {
            size_t value = 0;
            for (size_t bit = 0; bit < bitCount; ++bit)
            {
                if (data[bitIndex / bitCount] & (1 << (bitIndex % bitCount)))
                {
                    value |= (1ULL << bit);
                }
                ++bitIndex;
            }
            indexes.push_back(value);
        }
    }

    // Overload << operator for printing
    friend ostream &operator<<(ostream &os, const VariableBitContainer &varVal)
    {
        os << "Min Bits To Store: " << static_cast<int>(varVal.getMinBitsToStoreIndex()) << ", indexes: [";
        for (size_t i = 0; i < varVal.indexes.size(); ++i)
        {
            os << varVal.indexes[i];
            if (i < varVal.indexes.size() - 1)
            {
                os << ", ";
            }
        }
        os << "]";
        return os;
    }
};

// Pack function that handles odd number of bytes by padding with zero
vector<uint16_t> group16(vector<uint8_t> bytes)
{
    // Pad with an extra byte if the number of bytes is odd
    if (bytes.size() % 2 != 0)
    {
        bytes.push_back(0); // Append 0 byte
    }

    vector<uint16_t> pairs;
    pairs.reserve(bytes.size() / 2); // Reserve space for efficiency

    // Pack pairs of two uint8_t values into uint16_t
    for (size_t i = 0; i < bytes.size(); i += 2)
    {
        uint16_t pair = (static_cast<uint16_t>(bytes[i]) << 8) | bytes[i + 1];
        pairs.push_back(pair);
    }

    // Output the size of the pairs vector
    return pairs;
}

map<uint16_t, VariableBitContainer> pack16(vector<uint8_t> bytes)
{
    println("Original size: " + to_string(bytes.size() * (sizeof(uint8_t))));

    vector<uint16_t> groupedBytes = group16(bytes);

    // Create a map to store the packed data
    map<uint16_t, VariableBitContainer> packed;

    // Iterate over the grouped bytes
    for (size_t i = 0; i < groupedBytes.size(); i++)
    // Check if the current grouped byte is already in the packed map
    {
        if (packed.find(groupedBytes[i]) == packed.end())
        {
            // If not, add a new VariableBitContainer object with the occurrence count
            packed[groupedBytes[i]] = VariableBitContainer();
        }

        // Add the current index to the VariableBitContainer object's values
        packed[groupedBytes[i]].addIndex(i);
    }

    // Print the packed map and print its size in bytes
    size_t sizeInBytes = 0;
    for (const auto &entry : packed)
    {
        sizeInBytes += entry.second.getSizeInBytes();
    }
    size_t totalSizeInBytes = sizeInBytes + packed.size() * sizeof(uint16_t);
    println("Total size in bytes: " + to_string(totalSizeInBytes));
    println("Result size (of original): " + to_string(100.0 * (double)totalSizeInBytes / (double)bytes.size()) + "%");

    // Return the packed map
    return packed;
}

void printMap(const map<uint16_t, VariableBitContainer> &myMap)
{
    for (const auto &entry : myMap)
    {
        cout << "Key: " << entry.first << ", Value: " << entry.second << endl;
    }
}

vector<uint8_t> readFileToVector(const string &filePath)
{
    ifstream file(filePath, ios::binary);

    // Check if the file was opened successfully
    if (!file)
    {
        cerr << "Could not open the file: " << filePath << endl;
        return {};
    }

    // Use iterators to read the file content into a vector
    vector<uint8_t> buffer((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    return buffer;
}

int main()
{
    // vector<uint8_t> rawBytes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    // vector<uint8_t> rawBytes = {255, 254, 253, 252, 251, 250, 249, 248, 247, 246, 245, 244, 243, 242, 241, 240, 239, 238};
    // vector<uint8_t> rawBytes = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

    // vector<uint8_t> rawBytes = readFileToVector("./cat.jpg");
    vector<uint8_t> rawBytes = generateRandomBytes(65536);

    map<uint16_t, VariableBitContainer> packed = pack16(rawBytes);
}
