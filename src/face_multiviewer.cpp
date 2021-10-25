#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "config.h"
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


void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void mouseCallback(GLFWwindow* window, double xPos, double yPos);
void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
void processInput(GLFWwindow *window);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void overall2DetailedMode();
void detailed2OverallMode();
void helpMarker(const char* desc);
std::vector<float> photoPts2ScrPts(const std::vector<float> &photoPts, float height, float width, RotateType rotateType);

SceneMode sceneMode = SceneMode_Overall;

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const float LANDMARK_SIZE = 0.002;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
Camera savedCamera = camera;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// time gap
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// picked
const unsigned int NO_PICKED_FACE = 255;
const unsigned int NO_PICKED_LANDMARK = 255 * 255;
unsigned int iPickedFace = NO_PICKED_FACE;
unsigned int iPickedLandmark = NO_PICKED_LANDMARK;
int iExptFace = 0;
glm::vec2 landmarkOffset = glm::vec2(0.0, 0.0);

// trans matrix
glm::mat4 matFaceModel;

// pure color
const glm::vec3 RED = glm::vec3(1.0f, 0.0f, 0.0f);
const glm::vec3 YELLOW = glm::vec3(1.0f, 1.0f, 0.0f);
const glm::vec3 LANDMARK_COLOR = RED;
const glm::vec3 PICKED_LANDMARK_COLOR = YELLOW;

//	DataManager *g_pDataManager = new DataManager("D:/database/face_zzm/project");
DataManager *g_pDataManager = nullptr;
RenderManager *g_pRenderManager = nullptr;

// change landmark Log
bool g_bSelectLandmark = false;
std::vector<std::string> g_aChangeLog;
std::string g_sLog;

int main()
{
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
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetScrollCallback(window, scrollCallback);
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

	/* shader */
	Shader faceShader(SHADER_DIR"face.vs", SHADER_DIR"face.fs");
	Shader camShader(SHADER_DIR"cam.vs", SHADER_DIR"cam.fs");
	Shader quadShader(SHADER_DIR"quad.vs", SHADER_DIR"quad.fs");
	Shader cursorShader(SHADER_DIR"cursor.vs", SHADER_DIR"cursor.fs");
	Shader pointsShader(SHADER_DIR"points.vs", SHADER_DIR"points.fs");
	Shader lineShader(SHADER_DIR"line.vs", SHADER_DIR"line.fs");

	g_pDataManager = new DataManager("/home/bemfoo/Data/face_zzm/project");
	// g_pDataManager = new DataManager("D:/database/face_zzm/project-simple");	// fewer samples for faster debug
	g_pRenderManager = new RenderManager();

	unsigned int nFaces = g_pDataManager->getFaces();
	const Model *faceModel = g_pDataManager->getModel();
	const std::vector < Eigen::Matrix<float, 3, 4>> &aProjMatrices = g_pDataManager->getProjMatrices();
	const std::vector<Eigen::Matrix4f> &aInvTransMatrices = g_pDataManager->getInvTransMatrices();
	const std::vector<Eigen::Vector3f> &aCamPositions = g_pDataManager->getCamPositions();
	std::vector<std::vector<float>> &aLandmarkCoordsSets = g_pDataManager->getLandmarkCoordsSets();
	const std::vector<Texture> &aTextures = g_pDataManager->getTextures();

	faceShader.use();
	std::string strLightPos;
	for (unsigned int iFace = 0; iFace < nFaces; iFace++)
	{
		strLightPos = "lightPos[" + std::to_string(iFace) + "]";
		glUniform3fv(glGetUniformLocation(faceShader.ID, strLightPos.c_str()), 1, aCamPositions[iFace].data());
	}

	glm::mat4 projection, view;
	Eigen::Matrix4f trans, model;
	Eigen::Matrix4f matPhotoScale, matCamScale;
	double faceWidth = g_pDataManager->getWidth();
	double faceHeight = g_pDataManager->getHeight();
	double f = g_pDataManager->getF();
	double invF = 1 / f;
	double cx = g_pDataManager->getCx();
	double cy = g_pDataManager->getCy();

	double faceScale = 1 / f;

	matPhotoScale << faceWidth * faceScale, 0.0f, 0.0f, 0.0f,
				     0.0f, faceHeight * faceScale, 0.0f, 0.0f,
				     0.0f, 0.0f, 1.0f, 0.0f,
				     0.0f, 0.0f, 0.0f, 1.0f;

	matCamScale << 0.1f, 0.0f, 0.0f, 0.0f,
				   0.0f, 0.1f, 0.0f, 0.0f,
				   0.0f, 0.0f, 0.1f, 0.0f,
				   0.0f, 0.0f, 0.0f, 1.0f;

	faceShader.use();
	faceShader.setMat4("model", glm::mat4(1.0f));

	quadShader.use();
	quadShader.setVec3("PureColor", LANDMARK_COLOR);	// mark landmarks as red

	int scrWidth, scrHeight;
	bool guiActive = true;
	while (!glfwWindowShouldClose(window))
	{

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		processInput(window);
		glfwGetWindowSize(window, &scrWidth, &scrHeight);
		glViewport(0, 0, scrWidth, scrHeight);

		if (sceneMode == SceneMode_Overall)
		{
			glDisable(GL_SCISSOR_TEST);
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Multi Face Viewer - Overall Mode", &guiActive, ImGuiWindowFlags_MenuBar);	
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Close", "Esc")) 
					{
						glfwSetWindowShouldClose(window, true);
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			ImGui::Text(
				"Hold `Left_Shift` and move with `W`/`A`/`S`/`D` or mouse.\n"
				"(first person perspective)"
			);
			ImGui::InputInt("Face Id", &iExptFace, 1, 100, ImGuiInputTextFlags_CharsDecimal);
			if (ImGui::Button("Edit Landmarks"))
			{
				ImGui::End();
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
				iPickedFace = iExptFace;
				overall2DetailedMode();
				continue;
			}
			ImGui::SameLine(); 
			helpMarker("Or you can click with cursor pointing at expected face");
			ImGui::Text("If moving cursor to expected face, then its landmarks will show up.");
			ImGui::End();

			quadShader.use();
			quadShader.setBool("shouldRotate", false);
			quadShader.setInt("RenderMode", RenderMode_ColorPicking);
			projection = glm::perspective(glm::radians(camera.Zoom), (float)scrWidth / (float)scrHeight, 0.1f, 100.0f);
			view = camera.GetViewMatrix();
			quadShader.setMat4("projection", projection);
			quadShader.setMat4("view", view);

			// render color-picking
			glClearColor(1.0f, NO_PICKED_FACE / 255.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			for (unsigned int iFace = 0; iFace < nFaces; iFace++)
			{
				trans = aInvTransMatrices[iFace];
				model = trans * matPhotoScale;
				glUniformMatrix4fv(glGetUniformLocation(quadShader.ID, "model"), 1, GL_FALSE, model.data());
				quadShader.setFloat("FaceIdx", (float)iFace);
				quadShader.setFloat("LandmarkIdx", 255.0*255.0);
				g_pRenderManager->RenderQuad(RotateType_No);
			}

			// store color
			unsigned char ucScrData[3];
			glReadPixels(scrWidth / 2.0, scrHeight / 2.0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, ucScrData);
			iPickedFace = ucScrData[0];
			if (iPickedFace != NO_PICKED_FACE)	// not background 
			{
				std::cout << "Choose Face: " << iPickedFace << std::endl;
				std::vector<float> aLandmarkCoords = aLandmarkCoordsSets[iPickedFace];
				if (aLandmarkCoords.size() == 0)
					std::cout << "This face has no landmark." << std::endl;

				for (unsigned int iLandmark = 0; iLandmark < aLandmarkCoords.size() / 2; iLandmark++)
				{
					Eigen::Matrix4f landmarkTransMatrix;
					landmarkTransMatrix <<
						LANDMARK_SIZE, 0.0f, 0.0f, (aLandmarkCoords[iLandmark * 2] - faceWidth / 2) * faceScale * 2,
						0.0f, LANDMARK_SIZE, 0.0f, (aLandmarkCoords[iLandmark * 2 + 1] - faceHeight / 2) * faceScale * 2,
						0.0f, 0.0f, 0.999999f, 0.0f,
						0.0f, 0.0f, 0.0f, 1.0f;
					model = aInvTransMatrices[iPickedFace] * landmarkTransMatrix;

					glUniformMatrix4fv(glGetUniformLocation(quadShader.ID, "model"), 1, GL_FALSE, model.data());
					quadShader.setFloat("LandmarkIdx", iLandmark);
					g_pRenderManager->RenderQuad(RotateType_No);
				}

				// store color
				unsigned char ucScrData[3];
				glReadPixels(scrWidth / 2.0, scrHeight / 2.0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, ucScrData);
				iPickedLandmark = ucScrData[1] + ucScrData[2] * 255;
				if (iPickedLandmark != 255 * 255)	// not background (black)
					std::cout << "Choose Landmark: " << iPickedLandmark << std::endl;
			}

			// render scene
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			faceShader.use();
			faceShader.setMat4("projection", projection);
			faceShader.setMat4("view", view);
			faceShader.setVec3("viewPos", camera.Position);
			faceModel->Draw(faceShader);

			camShader.use();
			camShader.setMat4("projection", projection);
			camShader.setMat4("view", view);

			quadShader.use();
			quadShader.setInt("RenderMode", RenderMode_Texture);
			for (unsigned int iFace = 0; iFace < nFaces; iFace++)
			{
				trans = aInvTransMatrices[iFace];

				quadShader.use();
				model = trans * matPhotoScale;
				glUniformMatrix4fv(glGetUniformLocation(quadShader.ID, "model"), 1, GL_FALSE, model.data());
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, aTextures[iFace].getId());
				g_pRenderManager->RenderQuad(RotateType_No);

				camShader.use();
				model = trans * matCamScale;
				glUniformMatrix4fv(glGetUniformLocation(camShader.ID, "model"), 1, GL_FALSE, model.data());
				g_pRenderManager->RenderCube();
			}

			cursorShader.use();
			g_pRenderManager->RenderCursor();
		}
		else if (sceneMode == SceneMode_Detailed)
		{
			auto itLandmarkCoords = aLandmarkCoordsSets.begin() + iPickedFace;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin("Multi Face Viewer - Detailed Mode", &guiActive, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Save", "Ctrl+S"))
					{
						g_pDataManager->saveLandmarks(iPickedFace);
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
				detailed2OverallMode();
				continue;
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
			ImGui::Text("Current Chosen Face Id: %d.", iPickedFace);
			ImGui::InputInt("Next Face Id", &iExptFace, 1, 100, ImGuiInputTextFlags_CharsDecimal);
			if (ImGui::Button("Change Face"))
			{
				iPickedFace = iExptFace;
			}
			ImGui::Text("Hold `Mouse Left Butoon` to change landmark coordinates.");
			ImGui::Text("If one landmark (original is ");
			ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "RED");
			ImGui::SameLine(); ImGui::Text(") was chosen, it would turn into ");
			ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "YELLOW");
			ImGui::SameLine(); ImGui::Text(".");
			ImGui::Text("After move, DO NOT forget to SAVE using `Ctrl-S`.");
			if (iPickedLandmark < itLandmarkCoords->size())
				ImGui::Text("Current Chosen Landmark Id: %d.", iPickedLandmark);
			else
				ImGui::Text("Current No Chosen Landmark.");
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "Change Log");
			ImGui::BeginChild("Scrolling");
			for (auto iLog = g_aChangeLog.rbegin(); iLog < g_aChangeLog.rend(); iLog++)
				ImGui::Text("%s", iLog->c_str());
			ImGui::EndChild();
			if (ImGui::Button("Save New Landmarks"))
			{
				g_pDataManager->saveLandmarks(iPickedFace);
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
				std::ofstream out(std::to_string(iPickedFace) + "-" + std::string(date) + ".log");
				for (auto iLog = g_aChangeLog.begin(); iLog < g_aChangeLog.end(); iLog++)
					out << *iLog << "\n";
				out.close();
			}

			ImGui::End();

			glEnable(GL_SCISSOR_TEST);
			glViewport(0, 0, scrWidth / 2, scrHeight);
			glScissor(0, 0, scrWidth / 2, scrHeight);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			projection = glm::perspective(glm::radians(camera.Zoom), (float)scrWidth / (float)scrHeight / 2, 0.1f, 100.0f);
			view = camera.GetViewMatrix();
			glm::mat4 invProjView = glm::inverse(projection * view);

			faceShader.use();
			faceShader.setMat4("projection", projection);
			faceShader.setMat4("view", view);
			faceShader.setMat4("model", matFaceModel);
			faceShader.setVec3("viewPos", camera.Position);
			faceModel->Draw(faceShader);

			if (iPickedLandmark < itLandmarkCoords->size())
			{
				Eigen::Matrix4f invTransMat = aInvTransMatrices[iPickedFace];
				float x = (itLandmarkCoords->at(iPickedLandmark * 2) - faceWidth * 0.5 - cx) * invF;
				float y = (itLandmarkCoords->at(iPickedLandmark * 2 + 1) - faceHeight * 0.5 - cy) * invF;
				Eigen::Vector4f scrPoint(x, y, 1.0f, 1.0f);
				lineShader.use();
				lineShader.setMat4("projection", projection);
				lineShader.setMat4("view", view);
				glUniformMatrix4fv(glGetUniformLocation(lineShader.ID, "model"), 1, GL_FALSE, invTransMat.data());
				glUniform4f(glGetUniformLocation(lineShader.ID, "endPoint"), x, y, 1.0f, 1.0f);
				g_pRenderManager->RenderLine();
			}
		
			glViewport(scrWidth / 2, 0, scrWidth / 2, scrHeight);
			glScissor(scrWidth / 2, 0, scrWidth / 2, scrHeight);

			projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
			view = glm::lookAt(glm::vec3(0.0, 0.0, 5.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
			
			glm::mat4 m(1.0);
			m = glm::translate(m, glm::vec3(0.0f, 0.0f, 0.0f));
			quadShader.use();
			quadShader.setMat4("projection", projection);
			quadShader.setMat4("view", view);
			RotateType rotateType = aTextures[iPickedFace].getRotateType();
		
			// store color
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			unsigned char ucScrData[3];
			double xCursorPos, yCursorPos;
			glfwGetCursorPos(window, &xCursorPos, &yCursorPos);
			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			{
				if (iPickedLandmark < itLandmarkCoords->size())
				{
					if (g_bSelectLandmark == false)
					{
						auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
						struct tm* ptm = localtime(&tt);
						char date[10] = { 0 };
						sprintf(date, "[%02d.%02d.%02d] ", (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
						g_sLog = std::string(date);
						g_sLog += "(" + std::to_string((int)itLandmarkCoords->at(iPickedLandmark * 2))
							+ "," + std::to_string((int)itLandmarkCoords->at(iPickedLandmark * 2 + 1))
							+ ")->";
						g_bSelectLandmark = true;
					}

					if (rotateType == RotateType_CCW)
					{
						itLandmarkCoords->at(iPickedLandmark * 2) = (scrHeight - yCursorPos) / scrHeight * faceWidth;
						itLandmarkCoords->at(iPickedLandmark * 2 + 1) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceHeight;
					}
					else if(rotateType == RotateType_No)
					{
						itLandmarkCoords->at(iPickedLandmark * 2) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceWidth;
						itLandmarkCoords->at(iPickedLandmark * 2 + 1) = (scrHeight - yCursorPos) / scrHeight * faceHeight;
					}
					else
					{
						itLandmarkCoords->at(iPickedLandmark * 2) = (scrHeight - yCursorPos) / scrHeight * faceWidth;
						itLandmarkCoords->at(iPickedLandmark * 2 + 1) = (xCursorPos - scrWidth * 0.5) * 2.0 / scrWidth * faceHeight;
					}
				}
			}
			else if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE && g_bSelectLandmark)
			{
				g_sLog += "(" + std::to_string((int)itLandmarkCoords->at(iPickedLandmark * 2))
					+ "," + std::to_string((int)itLandmarkCoords->at(iPickedLandmark * 2 + 1))
					+ ")";
				g_aChangeLog.push_back(g_sLog);
				g_bSelectLandmark = false;
			}

			glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			quadShader.setInt("RenderMode", RenderMode_ColorPicking);
			quadShader.setFloat("FaceIdx", 0.0f);

			glm::mat4 quadModel(1.0f);
			std::vector<float> scrPts = photoPts2ScrPts(*itLandmarkCoords, faceHeight, faceWidth, rotateType);
			for (unsigned int iLandmark = 0; iLandmark < itLandmarkCoords->size() / 2; iLandmark++)
			{
				glm::mat4 quadModel(1.0f);
				quadModel = glm::translate(quadModel, glm::vec3(scrPts[iLandmark * 2], scrPts[iLandmark * 2 + 1], 0.0f));
				quadModel = glm::scale(quadModel, glm::vec3(0.01f, 0.01f, 1.1f));
				quadShader.setMat4("model", quadModel);
				quadShader.setFloat("LandmarkIdx", iLandmark);
				quadShader.use();
				g_pRenderManager->RenderQuad(RotateType_No);
			}

			if (xCursorPos > scrWidth * 0.5 && xCursorPos < scrWidth
				&& yCursorPos > 0.0 && yCursorPos < scrHeight)
			{
				glReadPixels(xCursorPos, scrHeight - yCursorPos, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, ucScrData);
				iPickedLandmark = ucScrData[1] + ucScrData[2] * 255;
				std::cout << (int)ucScrData[0] << " " << (int)ucScrData[1] << " " << (int)ucScrData[2] << std::endl;
				if (iPickedLandmark != NO_PICKED_LANDMARK)	// not background 
					std::cout << "Choose Landmark: " << iPickedLandmark << std::endl;
			}

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			quadShader.setInt("RenderMode", RenderMode_PureColor);
			for (unsigned int iLandmark = 0; iLandmark < itLandmarkCoords->size() / 2; iLandmark++)
			{
				glm::mat4 quadModel(1.0f);
				quadModel = glm::translate(quadModel, glm::vec3(scrPts[iLandmark * 2], scrPts[iLandmark * 2 + 1], 0.0f));
				quadModel = glm::scale(quadModel, glm::vec3(0.01f, 0.01f, 1.1f));
				quadShader.setMat4("model", quadModel);
				if (iPickedLandmark == iLandmark)
				{
					quadShader.setVec3("PureColor", PICKED_LANDMARK_COLOR);
					g_pRenderManager->RenderQuad(RotateType_No);
					quadShader.setVec3("PureColor", LANDMARK_COLOR);
				}
				else
				{
					g_pRenderManager->RenderQuad(RotateType_No);
				}
			}

			quadShader.setMat4("model", m);
			quadShader.setInt("RenderMode", RenderMode_Texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, aTextures[iPickedFace].getId());
			g_pRenderManager->RenderQuad(rotateType);
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


void processInput(GLFWwindow *window)
{
	if (sceneMode == SceneMode_Overall || 
		(sceneMode == SceneMode_Detailed && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS))
	{
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			camera.ProcessKeyboard(FORWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			camera.ProcessKeyboard(BACKWARD, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.ProcessKeyboard(LEFT, deltaTime);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.ProcessKeyboard(RIGHT, deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}


void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}


void mouseCallback(GLFWwindow* window, double xPos, double yPos)
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
		camera.ProcessMouseMovement(xOffset, yOffset);
	}

	if (sceneMode == SceneMode_Overall && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && iPickedFace != NO_PICKED_FACE)
	{
		overall2DetailedMode();
	}

}


void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	camera.ProcessMouseScroll(yOffset);
}


void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (sceneMode == SceneMode_Detailed && 
		( (key == GLFW_KEY_Q && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL) || 
		(key == GLFW_KEY_O && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)))
	{
		detailed2OverallMode();
	}

	if (sceneMode == SceneMode_Detailed)
	{
		if(key == GLFW_KEY_S && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)
			g_pDataManager->saveLandmarks(iPickedFace);

		if (key == GLFW_KEY_R && action == GLFW_RELEASE && mods == GLFW_MOD_CONTROL)
			camera = Camera();
	}

	if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}


void overall2DetailedMode()
{
	savedCamera = camera;
	camera = Camera();
	matFaceModel = glm::mat4(1.0f);
	sceneMode = SceneMode_Detailed;
}


void detailed2OverallMode()
{
	camera = savedCamera;
	sceneMode = SceneMode_Overall;
}

void helpMarker(const char* desc)
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

std::vector<float> photoPts2ScrPts(const std::vector<float> &photoPts, float height, float width, RotateType rotateType)
{
	std::vector<float> scrPts;
	for (unsigned int iPt = 0; iPt < photoPts.size() / 2; iPt++)
	{
		float xPos, yPos;
		if (rotateType == RotateType_CCW)
		{
			xPos = photoPts.at(iPt * 2 + 1) / height * 2 - 1.0f;
			yPos = photoPts.at(iPt * 2) / width * 2 - 1.0f;
		}
		else if (rotateType == RotateType_No)
		{
			xPos = photoPts.at(iPt * 2) / width * 2 - 1.0f;
			yPos = photoPts.at(iPt * 2 + 1) / height * 2 - 1.0f;
		}
		else
		{
			xPos = photoPts.at(iPt * 2 + 1) / height * 2 - 1.0f;
			yPos = 1.0f - photoPts.at(iPt * 2) / width * 2;
		}
		scrPts.push_back(xPos);
		scrPts.push_back(yPos);
	}
	return scrPts;
}