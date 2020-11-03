#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <codecvt>  // deprecated
#include <string>
#include <ios>
#include <locale>
#include <filesystem>
#include <algorithm>

namespace PMXLoader {

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
        float x;
        float y;
    };

    struct Vertex {
        float3 position;
        float3 normal;
        float2 uv;
    };

    struct Material {
        std::string name;
        std::string name_en;
        float4 diffuse;
        float3 specular;
        float specular_coef;
        float3 ambient;
        uint8_t drawing_mode;
        float4 edge_color;
        float edge_size;
        int normal_texture;
        int sphere_texture;
        uint8_t sphere_mode;
        bool sharing_toon;
        int toon_texture;
        std::string memo;
        int number_of_plane;
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
        std::array<char16_t, 1024> buffer{};
        std::string output;
        int size;

        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;

        in.read(reinterpret_cast<char*>(&size), sizeof(int));
        in.read(reinterpret_cast<char*>(&buffer), size);

        output = conv.to_bytes(buffer.data());

        return output;
    }

    std::tuple<
        std::vector<Vertex>,
        std::vector<int>,
        std::vector<std::filesystem::path>,
        std::vector<Material>> read_pmx(std::string filename) {
        std::filesystem::path basedir = std::filesystem::path(filename).remove_filename();
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
        int number_of_texture;
        int number_of_material;
        
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

        file.read(reinterpret_cast<char*>(&number_of_texture), sizeof(int));
        std::vector<std::filesystem::path> textures(number_of_texture);
        for (int j = 0; j < number_of_texture; ++j) {
            std::string p = read_wstring_from_pmx(file);
            std::replace(p.begin(), p.end(), '\\', '/');
            textures[j] = basedir / std::filesystem::path(p);
            //std::cout << textures[j] << std::filesystem::exists(textures[j]) << std::endl;
        }

        file.read(reinterpret_cast<char*>(&number_of_material), sizeof(int));
        std::vector<Material> materials(number_of_material);
        for (int j = 0; j < number_of_material; ++j) {
            materials[j].name = read_wstring_from_pmx(file);
            materials[j].name_en = read_wstring_from_pmx(file);
            file.read(reinterpret_cast<char*>(&materials[j].diffuse), sizeof(float4));
            file.read(reinterpret_cast<char*>(&materials[j].specular), sizeof(float3));
            file.read(reinterpret_cast<char*>(&materials[j].specular_coef), sizeof(float));
            file.read(reinterpret_cast<char*>(&materials[j].ambient), sizeof(float3));
            file.read(reinterpret_cast<char*>(&materials[j].drawing_mode), sizeof(uint8_t));
            file.read(reinterpret_cast<char*>(&materials[j].edge_color), sizeof(float4));
            file.read(reinterpret_cast<char*>(&materials[j].edge_size), sizeof(float));
            file.read(reinterpret_cast<char*>(&materials[j].normal_texture), property.texture_index_size);
            file.read(reinterpret_cast<char*>(&materials[j].sphere_texture), property.texture_index_size);
            file.read(reinterpret_cast<char*>(&materials[j].sphere_mode), sizeof(uint8_t));
            file.read(reinterpret_cast<char*>(&materials[j].sharing_toon), sizeof(bool));
            if (materials[j].sharing_toon) {
                uint8_t share_toon_texture;
                file.read(reinterpret_cast<char*>(&share_toon_texture), sizeof(uint8_t));
                materials[j].toon_texture = static_cast<int>(share_toon_texture);
            } else {
                switch (property.texture_index_size) {
                    case 1:
                        uint8_t tmp1;
                        file.read(reinterpret_cast<char*>(&tmp1), sizeof(tmp1));
                        materials[j].toon_texture = static_cast<int>(tmp1);
                        break;
                    case 2:
                        uint16_t tmp2;
                        file.read(reinterpret_cast<char*>(&tmp2), sizeof(tmp2));
                        materials[j].toon_texture = static_cast<int>(tmp2);
                        break;
                    case 4:
                        uint32_t tmp4;
                        file.read(reinterpret_cast<char*>(&tmp4), sizeof(tmp4));
                        materials[j].toon_texture = static_cast<int>(tmp4);
                        break;
                }
            }
            materials[j].memo = read_wstring_from_pmx(file);
            file.read(reinterpret_cast<char*>(&materials[j].number_of_plane), sizeof(int));
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

        return {vertices, planes, textures, materials};
    }

}