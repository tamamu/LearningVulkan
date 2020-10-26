#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <codecvt>  // deprecated
#include <string>
#include <ios>
#include <locale>

struct PMXProperty {
    int is_utf8;
    int additional_uv;
    int vertex_index_size;
    int texture_index_size;
    int material_index_size;
    int bone_index_size;
    int morph_index_size;
    int rigid_body_index_size;
};

struct float4 {
    float x;
    float y;
    float z;
    float w;
};

struct float3 {
    float x;
    float y;
    float z;
};

struct float2 {
    float u;
    float v;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

Vertex read_vertex_from_pmx(std::ifstream& in, int number_of_additional_uv, int bone_index_size) {
    Vertex v;
    std::vector<float4> additional_uv(number_of_additional_uv);
    char weight_transformation;

    in.read(reinterpret_cast<char*>(&v),
            sizeof(Vertex));
    in.read(reinterpret_cast<char*>(additional_uv.data()),
            number_of_additional_uv * sizeof(float4));
    in.read(reinterpret_cast<char*>(&weight_transformation),
            sizeof(char));

    switch(weight_transformation) {
        case 0://BDEF1
            in.seekg(bone_index_size, std::ios_base::cur);
            break;
        case 1://BDEF2
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            break;
        case 2://BDEF4
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            break;
        case 3://SDEF
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(bone_index_size, std::ios_base::cur);
            in.seekg(sizeof(float), std::ios_base::cur);
            in.seekg(sizeof(float3), std::ios_base::cur);
            in.seekg(sizeof(float3), std::ios_base::cur);
            in.seekg(sizeof(float3), std::ios_base::cur);
            break;
        default:
            std::cerr << number_of_additional_uv << std::endl;
            std::cerr << bone_index_size << std::endl;
            std::cerr << static_cast<int>(weight_transformation) << std::endl;
            throw std::runtime_error("unknown wt");
            std::cerr << static_cast<int>(weight_transformation) << ": unknown wt" << std::endl;
            break;
    }

    in.seekg(sizeof(float), std::ios_base::cur);

    return v;
}

std::string read_wstring_from_pmx(std::ifstream& in) {
    std::array<char16_t, 512> buffer;
    std::string output;
    int size;

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;

    in.read(reinterpret_cast<char*>(&size), sizeof(int));
    in.read(reinterpret_cast<char*>(&buffer), size);

    output = conv.to_bytes(buffer.data());

    return output;
}

std::tuple<std::vector<Vertex>, std::vector<int>> read_pmx(std::string filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    char header[4];
    float ver;
    char property_size;  // PMX2.0 -> 8
    char properties[8];  // byte[property_size]


    std::string model_name;
    std::string model_name_en;
    std::string comment;
    std::string comment_en;
    int number_of_vertex;
    int number_of_plane;
    
    file.seekg(0);
    file.read(header, 4*sizeof(char));
    file.read(reinterpret_cast<char*>(&ver), sizeof(float));
    file.read(reinterpret_cast<char*>(&property_size), sizeof(uint8_t));
    file.read(reinterpret_cast<char*>(&properties), 8*sizeof(uint8_t));
    PMXProperty property = {
        static_cast<int>(properties[0]),
        static_cast<int>(properties[1]),
        static_cast<int>(properties[2]),
        static_cast<int>(properties[3]),
        static_cast<int>(properties[4]),
        static_cast<int>(properties[5]),
        static_cast<int>(properties[6]),
        static_cast<int>(properties[7])
    };
    model_name = read_wstring_from_pmx(file);
    model_name_en = read_wstring_from_pmx(file);
    comment = read_wstring_from_pmx(file);
    comment_en = read_wstring_from_pmx(file);

    file.read(reinterpret_cast<char*>(&number_of_vertex), sizeof(int));
    std::vector<Vertex> vertices(number_of_vertex);
    for (int j = 0; j < number_of_vertex; ++j) {
        vertices[j] = read_vertex_from_pmx(file, property.additional_uv, property.bone_index_size);
    }

    file.read(reinterpret_cast<char*>(&number_of_plane), sizeof(int));
    std::vector<int> planes(number_of_plane);
    //file.read(reinterpret_cast<char*>(planes.data()), number_of_plane*property.vertex_index_size*sizeof(uint8_t));
    for (int j = 0; j < number_of_plane; ++j) {
        file.read(reinterpret_cast<char*>(&planes[j]), property.vertex_index_size*sizeof(uint8_t));
    }

    // std::cout << header << " " << ver << std::endl
    //           << static_cast<int>(property_size) << std::endl
    //           << "エンコード方式(0:UTF-16LE, 1:UTF-8) = "
    //           << static_cast<int>(properties[0]) << std::endl
    //           << "追加UV数 = "
    //           << property.additional_uv << std::endl
    //           << "頂点Indexサイズ = "
    //           << property.vertex_index_size << std::endl
    //           << "テクスチャIndexサイズ = "
    //           << static_cast<int>(properties[3]) << std::endl
    //           << "材質Indexサイズ = "
    //           << static_cast<int>(properties[4]) << std::endl
    //           << "ボーンIndexサイズ = "
    //           << property.bone_index_size << std::endl
    //           << "モーフIndexサイズ = "
    //           << static_cast<int>(properties[6]) << std::endl
    //           << "剛体Indexサイズ = "
    //           << static_cast<int>(properties[7]) << std::endl
    //           << model_name << std::endl
    //           << model_name_en << std::endl
    //           << comment << std::endl
    //           << comment_en << std::endl
    //           << number_of_vertex << std::endl
    //           << number_of_plane << std::endl;
    file.close();

    return {vertices, planes};
}