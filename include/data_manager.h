#ifndef DATA_MANAGER
#define DATA_MANAGER

#include "utils/file_utils.h"
#include "gl/model.h"
#include "gl/render_manager.h"
#include "gl/texture.h"
#include "config.h"
#include "tinyxml2.h"
#include <Eigen/Dense>
#include <boost/algorithm/string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <chrono>

namespace fs = std::filesystem;
using namespace tinyxml2;

const RotateType k_aRotTypes[] = {
	RotateType_CW, RotateType_CW, RotateType_CW, RotateType_CW, 
	RotateType_CW, RotateType_CCW, RotateType_CCW, RotateType_CW, 
	RotateType_CW, RotateType_CW, RotateType_CCW, RotateType_CCW, 
	RotateType_CCW, RotateType_CCW, RotateType_CCW, RotateType_CCW, 
	RotateType_CCW, RotateType_CW, RotateType_CW, RotateType_CCW,
	RotateType_CCW, RotateType_CCW, RotateType_CCW, RotateType_CCW
};

class DataManager
{
public:
	DataManager(const std::string &rootDir) :
		m_pathRootDir(fs::path(rootDir)),
		m_dirFacialLdmk(m_pathRootDir / "face_landmarks"),
		m_dirEarLdmk(m_pathRootDir / "ear_landmarks"),
		m_pathPhotoDir(m_pathRootDir / "image"),
		m_pathXml(m_pathRootDir / "cam_scale.xml"),
		m_pathModel(m_pathRootDir / "photoscan_scale.ply")
	{
		loadCamInfo();
		loadLandmarks();
		loadTextures();
	}

	void loadModel()
	{
		m_model = new Model(m_pathModel.string());
		for(auto& mesh : m_model->meshes)
		{
			for(auto& v : mesh.vertices)
			{
				v.position_ = v.position_ / static_cast<float>(m_scale);
			}
		}
		m_model->setup();
	}

	void saveLandmarks(unsigned int iPickedFace) const
	{
		std::cout << "save landmark from face " << iPickedFace << std::endl;

		auto landmarkCoords = m_aLandmarkCoordsSets[iPickedFace];
		auto now = std::chrono::system_clock::now();
		time_t tt = std::chrono::system_clock::to_time_t(now);
		std::string strTime = ctime(&tt);
		strTime = strTime.substr(4, 3) + "_" + strTime.substr(9, 1) + "_" + strTime.substr(11, 8);
		fs::path landmarkFile = m_dirFacialLdmk / (file_utils::Id2Str(iPickedFace) + ".txt");
		if(fs::exists(landmarkFile))
		{
			fs::rename(landmarkFile, fs::path(m_dirFacialLdmk / (file_utils::Id2Str(iPickedFace)
				+ "_" + strTime + ".backup")));
			std::ofstream out(landmarkFile.string());
			for (int i = 0; i < N_FACIAL_LDMKS; ++i)
			{
				out << std::scientific << std::setprecision(19) 
					<< landmarkCoords[i * 2] << " " << landmarkCoords[i * 2 + 1] << "\n";
			}
			out.close();
		}
		landmarkFile = m_dirEarLdmk / (file_utils::Id2Str(iPickedFace) + ".txt");

		if(fs::exists(landmarkFile))
		{
			fs::rename(landmarkFile, fs::path(m_dirEarLdmk / (file_utils::Id2Str(iPickedFace)
				+ "_" + strTime + ".backup")));
			std::ofstream out(landmarkFile.string());
			for (int i = N_FACIAL_LDMKS; i < N_LANDMARKS; ++i)
			{
				out << std::scientific << std::setprecision(19) 
					<< landmarkCoords[i * 2] << " " << landmarkCoords[i * 2 + 1] << "\n";
			}
			out.close();
		}
	}

	const Model *getModel() const { return m_model; }

	double getF() const { return m_f; }
	double getCx() const { return m_cx; }
	double getCy() const { return m_cy; }
	double getWidth() const { return m_width; }
	double getHeight() const { return m_height; }
	unsigned int getFaces() const { return m_nFaces; }

	const std::vector<Eigen::Matrix<float, 3, 4>>& getProjMatrices() const { return m_aProjMatrices; }
	const std::vector<Eigen::Matrix4f>& getInvTransMatrices() const { return m_aInvTransMatrices; }
	const std::vector<Eigen::Vector3f>& getCamPositions() const { return m_aCamPositions; }
	std::vector<std::vector<float>>& getLandmarkCoordsSets() { return m_aLandmarkCoordsSets; }
	const std::vector<Texture>& getTextures() const { return m_aTextures; }

	void bindTextures()
	{
		for (auto& tex : m_aTextures)
		{
			glGenTextures(1, &tex.id);
			std::cout << "Bind texture " << tex.id << std::endl;

			glBindTexture(GL_TEXTURE_2D, tex.id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex.width, tex.height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex.data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(tex.data);
		}
	}

	fs::path m_pathRootDir;
	fs::path m_dirFacialLdmk;
	fs::path m_dirEarLdmk;
	fs::path m_pathPhotoDir;
	fs::path m_pathModel;
	fs::path m_pathXml;

	Model *m_model;

	// camera infomation
	double m_f;
	double m_cx;
	double m_cy;
	double m_width;
	double m_height;
	double m_scale;
	unsigned int m_nFaces;
	std::vector<Eigen::Matrix<float, 3, 4> > m_aProjMatrices;
	std::vector<Eigen::Matrix<float, 3, 4> > m_aTransMatrices;
	std::vector<Eigen::Matrix4f> m_aInvTransMatrices;
	std::vector<Eigen::Vector3f> m_aCamPositions;

	std::vector<std::vector<float>> m_aLandmarkCoordsSets;
	std::vector<Texture> m_aTextures;

private:

	int loadCamInfo()
	{
		std::cout << "Load camera information." << std::endl;

		Eigen::Matrix4f T_model = Eigen::Matrix4f::Identity();

		m_aProjMatrices.clear();
		m_aTransMatrices.clear();
		m_aInvTransMatrices.clear();
		m_aCamPositions.clear();

		std::vector<double> sensor_f;
		std::vector<double> sensor_cx;
		std::vector<double> sensor_cy;
		std::vector<double> sensor_w;
		std::vector<double> sensor_h;

		std::vector<int> sensor_idxs;

		int sensorNum;

		XMLDocument doc;
		doc.LoadFile(m_pathXml.string().c_str());

		XMLElement *root = doc.RootElement();
		XMLElement *chunk = root->FirstChildElement("chunk");  //document->chunk->sensors
		if (chunk != NULL)
		{
			//model transform
			XMLElement *xml_transform = chunk->FirstChildElement("transform");
			if (xml_transform != NULL)
			{
				XMLElement* rot = xml_transform->FirstChildElement("rotation");
				if(rot)
				{
					const char* rotationText = xml_transform->FirstChildElement("rotation")->GetText();
					const char* translationText = xml_transform->FirstChildElement("translation")->GetText();

					char *rotation = const_cast<char *>(rotationText);
					char *splits;
					const char *d = " ";
					splits = strtok(rotation, d);
					int row = 0, col = 0;
					while (splits)
					{
						T_model(row, col) = atof(splits);
						col++;
						if (col == 3)
						{
							row++;
							col = 0;
						}
						splits = strtok(NULL, d);
					}

					char *translation = const_cast<char *>(translationText);
					splits = strtok(translation, d);
					for (int i = 0; i < 3; i++)
					{
						T_model(i, 3) = atof(splits);
						splits = strtok(NULL, d);
					}
				}
				else
				{
					m_scale = stod(xml_transform->FirstChildElement("scale")->GetText());
					std::cout << "Scale:" << m_scale << std::endl;
				}
			}


			// sensors
			XMLElement *xml_sensors = chunk->FirstChildElement("sensors");
			if (xml_sensors != NULL)
			{
				int sensorNum = atoi(xml_sensors->Attribute("next_id"));
				cout << "sensor nums: " << sensorNum << endl;

				// intial sensor size
				sensor_f.resize(sensorNum);
				sensor_cx.resize(sensorNum);
				sensor_cy.resize(sensorNum);
				sensor_w.resize(sensorNum);
				sensor_h.resize(sensorNum);

				XMLElement *xml_sensor = xml_sensors->FirstChildElement("sensor");
				for (int i = 0; i < sensorNum; i++)
					while (NULL != xml_sensor)
					{
						int sensorId = atoi(xml_sensor->Attribute("id"));
						//cout<<sensorId<<endl;

						XMLElement *xml_calibration = xml_sensor->FirstChildElement("calibration");
						XMLElement *xml_resolution = xml_calibration->FirstChildElement("resolution");

						XMLElement *xml_f = xml_calibration->FirstChildElement("f");
						XMLElement *xml_cx = xml_calibration->FirstChildElement("cx");
						XMLElement *xml_cy = xml_calibration->FirstChildElement("cy");

						sensor_w[sensorId] = atoi(xml_resolution->Attribute("width"));
						sensor_h[sensorId] = atoi(xml_resolution->Attribute("height"));
						sensor_f[sensorId] = atof(xml_f->GetText());
						sensor_cx[sensorId] = atof(xml_cx->GetText());
						sensor_cy[sensorId] = atof(xml_cy->GetText());

						//distoration parameters
						XMLElement *xml_k1 = xml_calibration->FirstChildElement("k1");
						XMLElement *xml_k2 = xml_calibration->FirstChildElement("k2");
						XMLElement *xml_p1 = xml_calibration->FirstChildElement("p1");
						XMLElement *xml_p2 = xml_calibration->FirstChildElement("p2");
						XMLElement *xml_k3 = xml_calibration->FirstChildElement("k3");

						xml_sensor = xml_sensor->NextSiblingElement("sensor");
					}
			}

			//cameras
			XMLElement *cameras = chunk->FirstChildElement("cameras");
			if (cameras != NULL)
			{
				m_nFaces = atoi(cameras->Attribute("next_id"));
				cout << "camera nums: " << m_nFaces << endl;

				sensor_idxs.resize(m_nFaces);

				XMLElement *xml_camera = cameras->FirstChildElement("camera");
				while (NULL != xml_camera)
				{

					Eigen::Matrix<float, 4, 4> T_camera;
					Eigen::Matrix<float, 3, 4> P;
					int camera_idx = atoi(xml_camera->Attribute("id"));
					int sensor_idx = atoi(xml_camera->Attribute("sensor_id"));
					//cout << "camera " << camera_idx << " using sensor " << sensor_idx << endl;


					const char* transformtmp = xml_camera->FirstChildElement("transform")->GetText();
					char *transform = const_cast<char *>(transformtmp);
					char *splits;
					const char *d = " ";
					splits = strtok(transform, d);
					int row = 0, col = 0;
					while (splits) {
						T_camera(row, col) = atof(splits);
						col++;
						if (col == 4) {
							row++;
							col = 0;
						}
						splits = strtok(NULL, d);

					}

					double f = sensor_f[sensor_idx];
					double cx = sensor_cx[sensor_idx];
					double cy = sensor_cy[sensor_idx];
					double w = sensor_w[sensor_idx];
					double h = sensor_h[sensor_idx];
					Eigen::Matrix<float, 3, 4> K;
					K << f, 0, w / 2.0 + cx, 0,
						0, f, h / 2.0 + cy, 0,
						0, 0, 1, 0;

					Eigen::Matrix<float, 4, 4> T = T_camera.inverse() * T_model.inverse();
					P = K * T;

					m_aProjMatrices.push_back(P);
					m_aTransMatrices.push_back(T.block(0, 0, 3, 4));
					m_aInvTransMatrices.push_back(T.inverse());

					Eigen::Matrix3f R = T.block(0, 0, 3, 3);
					Eigen::Vector3f t(T(0, 3), T(1, 3), T(2, 3));
					Eigen::Vector3f tmp = -R.transpose()*t;
					m_aCamPositions.push_back(Eigen::Vector3f(tmp[0], tmp[1], tmp[2]));

					sensor_idxs[camera_idx] = sensor_idx;

					xml_camera = xml_camera->NextSiblingElement("camera");
				}
			}
		}

		m_f = sensor_f[0];
		m_cx = sensor_cx[0];
		m_cy = sensor_cy[0];
		m_width = sensor_w[0];
		m_height = sensor_h[0];

		return 0;
	}

	void loadLandmarks()
	{
		std::cout << "Load landmarks." << std::endl;

		m_aLandmarkCoordsSets.resize(m_nFaces);
		for(auto& set : m_aLandmarkCoordsSets)
			set = std::vector<float>(N_LANDMARKS * 2, 0.f);

		if (!std::filesystem::exists(m_dirFacialLdmk))
		{
			std::cout << "Error: Directory " << m_dirFacialLdmk << " does not exist." << std::endl;
			return;
		}

		std::cout << m_dirFacialLdmk << std::endl;
		for (std::filesystem::directory_iterator it(m_dirFacialLdmk); 
			it != std::filesystem::directory_iterator(); it++)
		{
			if (it->path().extension() != ".txt")
				continue;

			std::string filename = it->path().string();
			std::cout << "Load landmarks from: " << filename << std::endl;
			unsigned int camera_id = file_utils::Str2Id(it->path().stem().string());

			std::ifstream in(filename);
			if (!in)
			{
				std::cout << "\nError: Can not open " << it->path().string() << "." << std::endl;
				return;
			}
			std::string line;
			int i = 0;
			while (std::getline(in, line))
			{
				if (line.empty() || line == "\n")
					continue;
				std::vector<std::string> words;
				boost::split(words, line, boost::is_any_of(" "));
				m_aLandmarkCoordsSets[camera_id][i * 2] = std::stof(words[0]);
				m_aLandmarkCoordsSets[camera_id][(i++) * 2 + 1] = std::stof(words[1]);
				// std::cout << m_aLandmarkCoordsSets[camera_id][(i-1) * 2] << " : " << 
				// 	m_aLandmarkCoordsSets[camera_id][(i-1) * 2 + 1] << std::endl;
			}
		}

		if (!std::filesystem::exists(m_dirEarLdmk))
		{
			std::cout << "Error: Diretcory " << m_dirEarLdmk << " does not exist." << std::endl;
			return;
		}

		for (std::filesystem::directory_iterator it(m_dirEarLdmk); it != std::filesystem::directory_iterator(); it++)
		{
			if (it->path().extension() != ".txt")
				continue;

			unsigned int camera_id = file_utils::Str2Id(it->path().stem().string());
			std::cout << "Cam: " << camera_id << std::endl;
			std::string filename = it->path().string();
			std::cout << "Load landmarks from: " << filename << std::endl;

			std::ifstream in(filename);
			if (!in)
			{
				std::cout << "\nError: Can not open " << it->path().string() << "." << std::endl;
				return;
			}
			std::string line;
			int i = 0;
			while (std::getline(in, line))
			{
				if (line.empty() || line == "\n")
					continue;
				std::vector<std::string> words;
				boost::split(words, line, boost::is_any_of(" "));
				m_aLandmarkCoordsSets[camera_id][(N_FACIAL_LDMKS + i) * 2] = std::stof(words[0]);
				m_aLandmarkCoordsSets[camera_id][(N_FACIAL_LDMKS + i++) * 2 + 1] = std::stof(words[1]);
			}
		}
		std::cout << "Done" << std::endl;

	}

	void loadTextures()
	{
		std::cout << "Load texture" << std::endl;
		m_aTextures.resize(N_VIEWS);
		for (auto i = 0; i < N_VIEWS; i++)
		{
			fs::path pathTexture = m_pathPhotoDir / (file_utils::Id2Str(i) + ".jpg");
			if(!fs::exists(pathTexture))
				pathTexture = m_pathPhotoDir / (file_utils::Id2Str(i) + ".JPG");
			std::cout << "Load texture: " << pathTexture.string() << std::endl;
			m_aTextures[i] = Texture(pathTexture.string());
		}
	}

};


#endif
