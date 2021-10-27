#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "data_manager.h"
#include "render_manager.h"
#include "camera.h"

#include <cstring>
#include <iostream>


enum RenderMode
{
	RenderMode_ColorPicking = 1,
	RenderMode_Texture = 2,
	RenderMode_PureColor = 3
};

enum SceneMode
{
	SceneMode_Overall = 1,
	SceneMode_Detailed = 2
};


void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);
void ProcessInput(GLFWwindow *window);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

bool DrawGui(GLFWwindow* window);

void Overall2DetailedMode();
void Detailed2OverallMode();

void HelpMarker(const char* desc);
std::vector<float> PhotoPts2ScrPts(const std::vector<float> &photoPts, float height, float width, RotateType rotateType);

SceneMode g_sceneMode = SceneMode_Overall;

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const float LANDMARK_SIZE = 0.002;

// camera
Camera g_cam;
Camera g_lastCam = g_cam;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// time gap
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// picked
const unsigned int NO_PICKED_FACE = 255;
const unsigned int NO_PICKED_LANDMARK = 255 * 255;
int g_iPickedView = NO_PICKED_FACE;
int g_iPickedLandmark = NO_PICKED_LANDMARK;
int g_iExptView = 0;
glm::vec2 landmarkOffset = glm::vec2(0.f, 0.f);

// pure color
const glm::vec3 RED = glm::vec3(1.f, 0.f, 0.f);
const glm::vec3 YELLOW = glm::vec3(1.f, 1.f, 0.f);
const glm::vec3 LANDMARK_COLOR = RED;
const glm::vec3 PICKED_LANDMARK_COLOR = YELLOW;

//	DataManager *g_pDataManager = new DataManager("D:/database/face_zzm/project");
DataManager *g_pDataManager = nullptr;
RenderManager *g_pRenderManager = nullptr;

glm::mat4 g_mProj;
glm::mat4 g_mView;

// change landmark Log
bool g_bSelectLandmark = false;
std::vector<std::string> g_aChangeLog;
std::string g_sLog;

int main()
{
	g_pDataManager = new DataManager("/home/bemfoo/Data/face_zzm/project");

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Face Multi Viewer", nullptr, nullptr);
	if (window == nullptr)
	{
		std::cerr << "[Error] Failed to create GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetCursorPosCallback(window, MouseCallback);
	glfwSetScrollCallback(window, ScrollCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cerr << "[Error] Failed to initialize GLAD." << std::endl;
		return EXIT_FAILURE;
	}

	stbi_set_flip_vertically_on_load(true);

	// init imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	// Shader
	Shader modelShader(SHADER_DIR"model.vs", SHADER_DIR"model.fs");
	Shader camShader(SHADER_DIR"cam.vs", SHADER_DIR"cam.fs");
	Shader quadShader(SHADER_DIR"quad.vs", SHADER_DIR"quad.fs");
	Shader pointsShader(SHADER_DIR"points.vs", SHADER_DIR"points.fs");
	Shader lineShader(SHADER_DIR"line.vs", SHADER_DIR"line.fs");

	g_pDataManager->bindTextures();
	g_pDataManager->loadModel();
	g_pRenderManager = new RenderManager();

	auto nViews = g_pDataManager->getFaces();
	const Model *faceModel = g_pDataManager->getModel();
	const std::vector < Eigen::Matrix<float, 3, 4>> &aProjMatrices = g_pDataManager->getProjMatrices();
	const std::vector<Eigen::Matrix4f> &aInvTransMatrices = g_pDataManager->getInvTransMatrices();
	const std::vector<Eigen::Vector3f> &aCamPositions = g_pDataManager->getCamPositions();
	std::vector<std::vector<float>> &aLandmarkCoordsSets = g_pDataManager->getLandmarkCoordsSets();
	const std::vector<Texture> &aTextures = g_pDataManager->getTextures();

	modelShader.use();
	std::string strLightPos;
	for (std::size_t i = 0; i < nViews; ++i)
	{
		strLightPos = "lightPos[" + std::to_string(i) + "]";
		glUniform3fv(glGetUniformLocation(modelShader.ID, strLightPos.c_str()), 1, aCamPositions[i].data());
	}

	Eigen::Matrix4f trans, model;
	Eigen::Matrix4f matPhotoScale;
	double faceWidth = g_pDataManager->getWidth();
	double faceHeight = g_pDataManager->getHeight();
	double f = g_pDataManager->getF();
	double invF = 1. / f;
	double cx = g_pDataManager->getCx();
	double cy = g_pDataManager->getCy();
	double faceScale = 1 / f;

	quadShader.use();
	quadShader.setVec3("PureColor", LANDMARK_COLOR);	// mark landmarks as red

	uByte ucScrData[3];
	int scrWidth, scrHeight;
	double xCursorPos, yCursorPos;

	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		ProcessInput(window);
		glfwGetWindowSize(window, &scrWidth, &scrHeight);
		glfwGetCursorPos(window, &xCursorPos, &yCursorPos);
		glViewport(0, 0, scrWidth, scrHeight);
		g_mView = g_cam.GetViewMatrix();

		if(DrawGui(window))
		{
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
			glfwPollEvents();
			continue;
		}
		
		if (g_sceneMode == SceneMode_Overall)
		{
			glDisable(GL_SCISSOR_TEST);

			g_mProj = glm::perspective(glm::radians(g_cam.Zoom), (float)scrWidth / (float)scrHeight, 0.1f, 5000.0f);

			quadShader.use();
			quadShader.setInt("RenderMode", RenderMode_ColorPicking);  
			quadShader.setMat4("Proj", g_mProj);
			quadShader.setMat4("View", g_mView);

			// Render color-picking
			glClearColor(1.0f, NO_PICKED_FACE / 255.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			for (int i = 0; i < N_VIEWS; ++i)
			{
				trans = aInvTransMatrices[i];
				quadShader.setMat4("Model", 
					utils::scale(trans, float(faceWidth * faceScale), float(faceHeight * faceScale), 1.f));
				quadShader.setFloat("FaceIdx", (float)i);
				quadShader.setFloat("LandmarkIdx", 255.0 * 255.0);
				g_pRenderManager->RenderQuad(RotateType_No);
			}

			if(xCursorPos > 0 && yCursorPos > 0 &&
				xCursorPos < scrWidth && yCursorPos < scrHeight)
			{
				std::cout << xCursorPos << " " << yCursorPos << std::endl;
				glReadPixels(xCursorPos, scrHeight - yCursorPos, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, ucScrData); 
				g_iPickedView = ucScrData[0]; // Value-red -> View id

				if (g_iPickedView != NO_PICKED_FACE)	// Not background 
				{
					std::cout << "Chosen view: " << g_iPickedView << std::endl;
					std::vector<float> aLandmarkCoords = aLandmarkCoordsSets[g_iPickedView];
					if (aLandmarkCoords.size() == 0)
						std::cout << "This face has no landmark." << std::endl;

					for (std::size_t i = 0; i < aLandmarkCoords.size() / 2; ++i)
					{
						Eigen::Matrix4f landmarkTransMatrix;
						landmarkTransMatrix <<
							LANDMARK_SIZE, 0.0f, 0.0f, (aLandmarkCoords[i * 2] - faceWidth / 2) * faceScale * 2,
							0.0f, LANDMARK_SIZE, 0.0f, (aLandmarkCoords[i * 2 + 1] - faceHeight / 2) * faceScale * 2,
							0.0f, 0.0f, 0.999999f, 0.0f,
							0.0f, 0.0f, 0.0f, 1.0f;
						model = aInvTransMatrices[g_iPickedView] * landmarkTransMatrix;

						quadShader.setMat4("Model", model);
						quadShader.setFloat("LandmarkIdx", i);
						g_pRenderManager->RenderQuad(RotateType_No);
					}
				}
			}
			else
			{
				g_iPickedView = NO_PICKED_FACE;
			}

			// For color-picking debug
			// {
			// 	ImGui::Render();
			// 	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			// 	glfwSwapBuffers(window);
			// 	glfwPollEvents();
			// 	continue;
			// }

			// Render scene
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Draw face model
			modelShader.use();
			modelShader.setMat4("Proj", g_mProj);
			modelShader.setMat4("View", g_mView);
			modelShader.setVec3("ViewPos", g_cam.Position);
			faceModel->Draw(modelShader);

			camShader.use();
			camShader.setMat4("Proj", g_mProj);
			camShader.setMat4("View", g_mView);

			quadShader.use();
			quadShader.setInt("RenderMode", RenderMode_Texture);
			for (int i = 0; i < nViews; ++i)
			{
				trans = aInvTransMatrices[i];

				quadShader.use();
				quadShader.setMat4("Model", 
					utils::scale(trans, float(faceWidth * faceScale), float(faceHeight * faceScale), 1.f));
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, aTextures[i].id);
				g_pRenderManager->RenderQuad(RotateType_No);

				camShader.use();
				camShader.setMat4("Model", trans);
				g_pRenderManager->RenderCube();
			}
		}
		else if (g_sceneMode == SceneMode_Detailed)
		{
			auto itLandmarkCoords = aLandmarkCoordsSets.begin() + g_iPickedView;

			// Left part: Model
			glEnable(GL_SCISSOR_TEST);
			glViewport(0, 0, scrWidth / 2, scrHeight);
			glScissor(0, 0, scrWidth / 2, scrHeight);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			g_mProj = glm::perspective(glm::radians(g_cam.Zoom), (float)scrWidth / (float)scrHeight / 2.f, 0.1f, 500.0f);
			glm::mat4 invProjView = glm::inverse(g_mProj * g_mView);

			modelShader.use();
			modelShader.setMat4("Proj", g_mProj);
			modelShader.setMat4("View", g_mView);
			modelShader.setVec3("ViewPos", g_cam.Position);
			faceModel->Draw(modelShader);

			if (g_iPickedLandmark < itLandmarkCoords->size())
			{
				Eigen::Matrix4f invTransMat = aInvTransMatrices[g_iPickedView];
				float x = (itLandmarkCoords->at(g_iPickedLandmark * 2) - faceWidth * 0.5 - cx) * invF;
				float y = (itLandmarkCoords->at(g_iPickedLandmark * 2 + 1) - faceHeight * 0.5 - cy) * invF;

				lineShader.use();
				lineShader.setMat4("Proj", g_mProj);
				lineShader.setMat4("View", g_mView);
				lineShader.setMat4("Model", invTransMat);
				lineShader.setVec4("EndPoint", x, y, 1.f, 1.f); // TODO
				g_pRenderManager->RenderLine();
			}
		
			// Right part: Landmarks
			glViewport(scrWidth / 2, 0, scrWidth / 2, scrHeight);
			glScissor(scrWidth / 2, 0, scrWidth / 2, scrHeight);
	
			g_mProj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
			g_mView = glm::lookAt(glm::vec3(0.0, 0.0, 5.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
			
			quadShader.use();
			quadShader.setMat4("Proj", g_mProj);
			quadShader.setMat4("View", g_mView);

			// store color
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			
			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			{
				if (g_iPickedLandmark < itLandmarkCoords->size())
				{
					if (g_bSelectLandmark == false)
					{
						auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
						struct tm* ptm = localtime(&tt);
						char date[10] = { 0 };
						sprintf(date, "[%02d.%02d.%02d] ", (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
						g_sLog = std::string(date);
						g_sLog += "(" + std::to_string((int)itLandmarkCoords->at(g_iPickedLandmark * 2))
							+ "," + std::to_string((int)itLandmarkCoords->at(g_iPickedLandmark * 2 + 1))
							+ ")->";
						g_bSelectLandmark = true;
					}

					if (k_aRotTypes[g_iPickedView] == RotateType_CCW)
					{
						itLandmarkCoords->at(g_iPickedLandmark * 2) = (scrHeight - yCursorPos) / scrHeight * faceWidth;
						itLandmarkCoords->at(g_iPickedLandmark * 2 + 1) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceHeight;
					}
					else if(k_aRotTypes[g_iPickedView] == RotateType_Invalid)
					{
						itLandmarkCoords->at(g_iPickedLandmark * 2) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceWidth;
						itLandmarkCoords->at(g_iPickedLandmark * 2 + 1) = (scrHeight - yCursorPos) / scrHeight * faceHeight;
					}
					else
					{
						itLandmarkCoords->at(g_iPickedLandmark * 2) = (scrHeight - yCursorPos) / scrHeight * faceWidth;
						itLandmarkCoords->at(g_iPickedLandmark * 2 + 1) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceHeight;
					}
				}
			}
			else if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE && g_bSelectLandmark)
			{
				g_sLog += "(" + std::to_string((int)itLandmarkCoords->at(g_iPickedLandmark * 2))
					+ "," + std::to_string((int)itLandmarkCoords->at(g_iPickedLandmark * 2 + 1))
					+ ")";
				g_aChangeLog.push_back(g_sLog);
				g_bSelectLandmark = false;
			}

			// Color-picking
			glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			quadShader.setInt("RenderMode", RenderMode_ColorPicking);
			quadShader.setFloat("FaceIdx", 0.0f); // Irrelevant

			glm::mat4 quadModel(1.0f);
			std::vector<float> scrPts = PhotoPts2ScrPts(*itLandmarkCoords, faceHeight, faceWidth, k_aRotTypes[g_iPickedView]);
			for (int i = 0; i < itLandmarkCoords->size() / 2; ++i)
			{
				quadShader.use();
				glm::mat4 quadModel(1.0f);
				quadModel = glm::translate(quadModel, glm::vec3(scrPts[i * 2], scrPts[i * 2 + 1], 0.0f));
				quadModel = glm::scale(quadModel, glm::vec3(0.01f, 0.01f, 1.1f));
				quadShader.setMat4("Model", quadModel);
				quadShader.setFloat("LandmarkIdx", static_cast<float>(i));
				g_pRenderManager->RenderQuad(RotateType_No);
			}

			if (xCursorPos > scrWidth * 0.5 && xCursorPos < scrWidth
				&& yCursorPos > 0.0 && yCursorPos < scrHeight) // Valid 
			{
				glReadPixels(xCursorPos, scrHeight - yCursorPos, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, ucScrData);
				g_iPickedLandmark = ucScrData[1] + ucScrData[2] * 255;
				std::cout << (int)ucScrData[0] << " " << (int)ucScrData[1] << " " << (int)ucScrData[2] << std::endl;
				if (g_iPickedLandmark != NO_PICKED_LANDMARK)	// not background 
					std::cout << "Choose Landmark: " << g_iPickedLandmark << std::endl;
			}

			// Draw landmarks
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			quadShader.setInt("RenderMode", RenderMode_PureColor);
			for (int i = 0; i < itLandmarkCoords->size() / 2; ++i)
			{
				glm::mat4 quadModel(1.0f);
				quadModel = glm::translate(quadModel, glm::vec3(scrPts[i * 2], scrPts[i * 2 + 1], 0.0f));
				quadModel = glm::scale(quadModel, glm::vec3(0.01f, 0.01f, 1.1f));
				quadShader.setMat4("Model", quadModel);
				if (g_iPickedLandmark == i)
				{
					quadShader.setVec3("PureColor", PICKED_LANDMARK_COLOR);
					g_pRenderManager->RenderQuad(RotateType_No);
				}
				else
				{
					quadShader.setVec3("PureColor", LANDMARK_COLOR);
					g_pRenderManager->RenderQuad(RotateType_No);
				}
			}

			quadShader.setMat4("Model", Eigen::Matrix4f::Identity());
			quadShader.setInt("RenderMode", RenderMode_Texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, aTextures[g_iPickedView].id);
			g_pRenderManager->RenderQuad(k_aRotTypes[g_iPickedView]);
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	return 0;
}


void ProcessInput(GLFWwindow *window)
{
	if (g_sceneMode == SceneMode_Overall || 
		(g_sceneMode == SceneMode_Detailed && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS))
	{
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			g_cam.ProcessKeyboard(FORWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			g_cam.ProcessKeyboard(BACKWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			g_cam.ProcessKeyboard(LEFT, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			g_cam.ProcessKeyboard(RIGHT, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}


void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}


void MouseCallback(GLFWwindow* window, double xPos, double yPos)
{
	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	float xOffset = xPos - lastX;
	float yOffset = lastY - yPos;
	lastX = xPos;
	lastY = yPos;

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
	{
		g_cam.ProcessMouseMovement(xOffset, yOffset);
	}

	if (g_sceneMode == SceneMode_Overall && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && g_iPickedView != NO_PICKED_FACE)
	{
		Overall2DetailedMode();
	}

}


void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	g_cam.ProcessMouseScroll(yOffset);
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (g_sceneMode == SceneMode_Detailed && 
		( (key == GLFW_KEY_Q && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL) || 
		(key == GLFW_KEY_O && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)))
	{
		Detailed2OverallMode();
	}

	if (g_sceneMode == SceneMode_Detailed)
	{
		if(key == GLFW_KEY_S && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)
			g_pDataManager->saveLandmarks(g_iPickedView);

		if (key == GLFW_KEY_R && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)
			g_cam = Camera();
	}

	if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}


void Overall2DetailedMode()
{
	g_lastCam = g_cam;
	g_cam = Camera();
	g_sceneMode = SceneMode_Detailed;
}


void Detailed2OverallMode()
{
	g_cam = g_lastCam;
	g_sceneMode = SceneMode_Overall;
}


void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}


std::vector<float> PhotoPts2ScrPts(const std::vector<float>& photoPts, float height, float width, RotateType rotateType)
{
	std::vector<float> scrPts;
	for (int i = 0; i < photoPts.size() / 2; ++i)
	{
		float xPos, yPos;
		if (rotateType == RotateType_CCW)
		{
			xPos = photoPts.at(i * 2 + 1) / height * 2 - 1.0f;
			yPos = photoPts.at(i * 2) / width * 2 - 1.0f;
		}
		else if(rotateType == RotateType_CW)
		{
			xPos = photoPts.at(i * 2 + 1) / height * 2 - 1.0f;
			yPos = 1.f - photoPts.at(i * 2) / width * 2;
		}
		else
		{
			std::cerr << "[ERROR] Wrong rotate type at point: " << i << std::endl;
			xPos = yPos = 0.f; 
		}

		scrPts.push_back(xPos);
		scrPts.push_back(yPos);
	}
	return scrPts;
}


bool DrawGui(GLFWwindow* window)
{
	bool guiActive = true;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	if(g_sceneMode == SceneMode_Overall)
	{
		ImGui::Begin("Face Multiviewer - Overall Mode", &guiActive, ImGuiWindowFlags_MenuBar);	
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Close", "Esc")) glfwSetWindowShouldClose(window, true);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		ImGui::Text(
			"Hold `Left_Shift` and move with `W`/`A`/`S`/`D` or mouse.\n"
			"(first person perspective)"
		);
		ImGui::InputInt("View id", &g_iExptView, 1, 100, ImGuiInputTextFlags_CharsDecimal);
		if (ImGui::Button("Edit Landmarks"))
		{
			ImGui::End();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			g_iPickedView = g_iExptView;
			Overall2DetailedMode();
			return true;
		}
		ImGui::SameLine(); 
		HelpMarker("Or you can click with cursor pointing at expected face");
		ImGui::Text("If moving cursor to expected face, then its landmarks will show up.");
	}
	else // Detailed Mode
	{
		ImGui::Begin("Multi Face Viewer - Detailed Mode", &guiActive, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save", "Ctrl+S"))
				{
					g_pDataManager->saveLandmarks(g_iPickedView);
				}

				if (ImGui::MenuItem("Close", "Esc")) 
				{
					glfwSetWindowShouldClose(window, true);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		if (ImGui::Button("Back to Overall Mode"))
		{
			ImGui::End();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			Detailed2OverallMode();
			return true;
		}
		ImGui::Text("`Ctrl-O`/`Ctrl-Q` to back to overall mode.");
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.0f, 0.78f, 0.55f, 1.0f), "I. Model (left)");
		ImGui::Text(
			"Hold `Left_Shift` and move with `W`/`A`/`S`/`D` or mouse.\n"
			"(first person perspective)"
		);
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.0f, 0.78f, 0.55f, 1.0f), "II. Landmark (right)");
		ImGui::Text("Current Chosen Face Id: %d.", g_iPickedView);
		ImGui::InputInt("Next Face Id", &g_iPickedView, 1, 100, ImGuiInputTextFlags_CharsDecimal);
		if(g_iPickedView < 0) g_iPickedView = 0;
		else if(g_iPickedView > 23) g_iPickedView = 23;

		ImGui::Text("Hold `Mouse Left Butoon` to change landmark coordinates.");
		ImGui::Text("If one landmark (original is ");
		ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "RED");
		ImGui::SameLine(); ImGui::Text(") was chosen, it would turn into ");
		ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "YELLOW");
		ImGui::SameLine(); ImGui::Text(".");
		ImGui::Text("After move, DO NOT forget to SAVE using `Ctrl-S`.");
		if (g_iPickedLandmark < N_LANDMARKS)
			ImGui::Text("Current Chosen Landmark Id: %d.", g_iPickedLandmark);
		else
			ImGui::Text("Current No Chosen Landmark.");
		ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Change Log");
		ImGui::BeginChild("Scrolling");
		for (auto iLog = g_aChangeLog.rbegin(); iLog < g_aChangeLog.rend(); iLog++)
			ImGui::Text("%s", iLog->c_str());
		ImGui::EndChild();
		if (ImGui::Button("Save New Landmarks"))
		{
			g_pDataManager->saveLandmarks(g_iPickedView);
		}
		ImGui::SameLine();
		if (ImGui::Button("Save Log"))
		{
			auto tt = std::chrono::system_clock::to_time_t
			(std::chrono::system_clock::now());
			struct tm* ptm = localtime(&tt);
			char date[60] = { 0 };
			sprintf(date, "%d-%02d-%02d-%02d-%02d-%02d",
				(int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
				(int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
			std::ofstream out(std::to_string(g_iPickedView) + "-" + std::string(date) + ".log");
			for (auto iLog = g_aChangeLog.begin(); iLog < g_aChangeLog.end(); iLog++)
				out << *iLog << "\n";
			out.close();
		}		
	}

	ImGui::End();
	return false;
}