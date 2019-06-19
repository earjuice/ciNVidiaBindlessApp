
#define DISABLE_FRAMERATE
//#define REVERSE_Z
#define USE_IMGUI

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/gl/Texture.h"
#include "cinder/CameraUi.h"
#include "cinder/Utilities.h"
#include "cinder/gl/TextureFormatParsers.h"
#ifdef USE_IMGUI
#include "CinderImGui.h"
#else
#include "cinder/params/Params.h"
#endif //USE_IMGUI
#include "Mesh.h"

#define SQRT_BUILDING_COUNT 100
#define TEXTURE_FRAME_COUNT 181
#define ANIMATION_DURATION 5.0f

using namespace ci;
using namespace ci::app;
using namespace std;

// This sample demonstrates using bindless rendering with GL_NV_shader_buffer_load and GL_NV_vertex_buffer_unified_memory.
// GL_NV_shader_buffer_load allows the program to pass a GPU pointer to the vertex shader to load uniforms directly from GPU memory.
// GL_NV_vertex_buffer_unified_memory allows the program to use GPU pointers to vertex and index data when making rendering calls.
// Both of these extensions can significantly reduce CPU L2 cache misses and pollution; this can dramatically speed up scenes with
// large numbers of draw calls.
//
// For more detailed information see http://www.nvidia.com/object/bindless_graphics.html
//
//
//
// Interesting pieces of code are annotated with "*** INTERESTING ***"
//
// The interesting code in this sample is in this file and Mesh.cpp
//
// Mesh::update() in Mesh.cpp contains the source code for getting the GPU pointers for vertex and index data
// Mesh::renderPrep() in Mesh.cpp sets up the vertex format
// Mesh::render() in Mesh.cpp does the actual rendering
// Mesh::renderFinish() in Mesh.cpp resets related state


class BindlessApp : public App {
public:
	BindlessApp();
	~BindlessApp() {}
	void setup() override;
	void mouseUp(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseWheel(MouseEvent event) override;
	void update() override;
	void draw() override;
	void resize() override;

	void updatePerMeshUniforms(float t);
	void InitBindlessTextures();


private:

	struct TransformUniforms
	{
		glm::mat4 ModelView;
		glm::mat4 ModelViewProjection;
		int32_t      UseBindlessUniforms;
	};

	struct PerMeshUniforms
	{
		float r, g, b, a, u, v;
	};

	void initRendering();

	void createBuilding(Mesh& mesh, ci::vec3 pos, ci::vec3 dim, ci::vec2 uv);
	void createGround(Mesh& mesh, ci::vec3 pos, ci::vec3 dim);
	void randomColor(float &r, float &g, float &b);

	//ci::gl::Texture2dRef	createFromDds(const DataSourceRef &dataSource, const ci::gl::Texture2d::Format &format = ci::gl::Texture2d::Format());

	//Camera
	CameraPersp mCam;
	CameraUi mCamUI;

	// Simple collection of meshes to render
	std::vector<Mesh>				m_meshes;
	//std::vector<ci::gl::VboMeshRef> m_vbo_meshes;

	// Shader stuff
	gl::GlslProgRef               m_shader;
	GLuint                        m_bindlessPerMeshUniformsPtrAttribLocation;

	// uniform buffer object (UBO) for tranform data
	GLuint                        m_transformUniforms;
	TransformUniforms             m_transformUniformsData;
	glm::mat4                  m_projectionMatrix;

	// uniform buffer object (UBO) for mesh param data
	GLuint                        m_perMeshUniforms;
	std::vector<PerMeshUniforms>  m_perMeshUniformsData;
	GLuint64EXT                   m_perMeshUniformsGPUPtr;

	//bindless texture handle
	ci::gl::Texture2dRef		  m_textureRefs[TEXTURE_FRAME_COUNT];
	GLuint64EXT*				  m_textureHandles;
	GLuint*						  m_textureIds;
	GLint					      m_numTextures;
	bool						  m_useBindlessTextures;
	int							  m_currentFrame;
	float						  m_currentTime;
	float						  avgfps;//avgfps
	// UI stuff
	//NvUIValueText*                m_drawCallsPerSecondText;
	float                m_drawCallsPerSecondText;
	bool                          m_useBindlessUniforms;
	bool                          m_updateUniformsEveryFrame;
	bool                          m_usePerMeshUniforms;

	// Timing related stuff
	float                         m_t;
	float                         m_minimumFrameDeltaTime;

#ifndef USE_IMGUI
	params::InterfaceGlRef mParams;
#endif //!USE_IMGUI
};

BindlessApp::BindlessApp() :
	m_drawCallsPerSecondText(0.f)
	, m_useBindlessUniforms(true)
	, m_updateUniformsEveryFrame(true)
	, m_usePerMeshUniforms(true)
	, m_useBindlessTextures(false)
	, m_currentFrame(0)
	, m_currentTime(0.0f)
{
#ifdef USE_IMGUI
	ui::initialize(ui::Options().fboRender(false));//ui::initialize();
#else
	mParams = params::InterfaceGl::create("BindlessApp", toPixels(ivec2(225, 180)));
	mParams->addSeparator();
	//mParams->addParam
	//ui::Text(("avg fps: " + ci::toString(getAverageFps())).c_str());
	// ui::Text((ci::toString(m_drawCallsPerSecondText) + " M draw calls/sec").c_str());
	mParams->addParam("fps: ", &avgfps).min(0.0f).max(10000.0f).step(0.01f);
	mParams->addParam("Mdraw/sec:  ", &m_drawCallsPerSecondText).min(0.0f).max(10000.0f).step(0.01f);
	//mParams->addText("fps: ", ci::toString(avgfps));
	// mParams->addParam("Mdraw/sec ", &fps).min(0.0f).max(2000.0f).step(0.01f);
	//mParams->addText("Mdraw/sec: ", ci::toString(m_drawCallsPerSecondText));
	mParams->addSeparator();
	mParams->addParam("Use bindless vertices/indices", &Mesh::m_enableVBUM);
	mParams->addParam("Use bindless uniforms", &m_useBindlessUniforms);
	mParams->addParam("Use per mesh uniforms", &m_usePerMeshUniforms);
	mParams->addParam("Update uniforms every frame", &m_updateUniformsEveryFrame);
	mParams->addParam("Set vertex format for each mesh", &Mesh::m_setVertexFormatOnEveryDrawCall);
	mParams->addParam("Use heavy vertex format", &Mesh::m_useHeavyVertexFormat);
	mParams->addParam("Use bindless textures", &m_useBindlessTextures);
	mParams->addParam("Draw calls per state", &Mesh::m_drawCallsPerState).min(1).max(20);
#endif //USE_IMGUI

	// Set up camera 
	const vec2 windowSize = toPixels(getWindowSize());
	mCam = CameraPersp(windowSize.x, windowSize.y, 45.0f, 0.01f, 100.0f);
	mCam.lookAt(vec3(-2.221f, 2.0f, 15.859f), vec3(0.0f, 2.0f, 0.0f));
#ifdef REVERSE_Z
	gl::enableDepthReversed();
	gl::clipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	mCam.setInfiniteFarClip(true);
#endif //REVERSE_Z
	// mCamUI.setCamera(&mCam);
	mCamUI = CameraUi(&mCam, getWindow(), -1);

#ifdef DISABLE_FRAMERATE
	gl::enableVerticalSync(false);
#endif //DISABLE_FRAMERATE
}

void BindlessApp::setup()
{
	//Initialize bindless scene
	initRendering();

}

////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::initRendering()
//
//    Sets up initial rendering state and creates meshes
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::initRendering() {

	console() << "GL_RENDERER " << glGetString(GL_RENDERER) << endl;
	console() << "GL_VERSION " << glGetString(GL_VERSION) << endl;
	// Check extensions; exit on failure
	if (!gl::isExtensionAvailable("GL_NV_vertex_buffer_unified_memory")) return;
	if (!gl::isExtensionAvailable("GL_NV_shader_buffer_load")) return;
	if (!gl::isExtensionAvailable("GL_EXT_direct_state_access")) return;
	if (!gl::isExtensionAvailable("GL_NV_bindless_texture")) return;


	// Create our pixel and vertex shader
	auto loadGlslProg = [&](const gl::GlslProg::Format& format) -> gl::GlslProgRef
	{
		const string names = format.getVertexPath().string() + " + " + format.getFragmentPath().string();
		gl::GlslProgRef glslProg;
		try {
			glslProg = gl::GlslProg::create(format);
		}
		catch (const Exception& ex) {
			//	CI_LOG_EXCEPTION(names, ex);
			ci::app::console() << "CI_LOG_EXCEPTION" << names << &ex << std::endl;
			quit();
		}
		return glslProg;
	};
	m_shader = loadGlslProg(gl::GlslProg::Format().vertex(loadAsset("shaders/simple_vertex.glsl")).fragment(loadAsset("shaders/simple_fragment.glsl")));
	m_bindlessPerMeshUniformsPtrAttribLocation = m_shader->getAttribLocation("bindlessPerMeshUniformsPtr");//, true);
	//LOGI("m_bindlessPerMeshUniformsPtrAttribLocation = %d", m_bindlessPerMeshUniformsPtrAttribLocation);
	ci::app::console() << "m_bindlessPerMeshUniformsPtrAttribLocation = " << m_bindlessPerMeshUniformsPtrAttribLocation << std::endl;

	// Set the initial view
	//m_transformer->setRotationVec(ci::vec3(30.0f * (3.14f / 180.0f), 30.0f * (3.14f / 180.0f), 0.0f));

	// Create the meshes
	m_meshes.resize(1 + SQRT_BUILDING_COUNT * SQRT_BUILDING_COUNT);
	//m_vbo_meshes.resize(1 + SQRT_BUILDING_COUNT * SQRT_BUILDING_COUNT);
	m_perMeshUniformsData.resize(m_meshes.size());

	// Create a mesh for the ground
	createGround(m_meshes[0], ci::vec3(0.f, -.001f, 0.f), ci::vec3(5.0f, 0.0f, 5.0f));
	//m_vbo_meshes[0] = ci::gl::VboMesh::create(ci::geom::Plane().size(vec2(5.f,5.f)) );
	// Create "building" meshes
	int32_t meshIndex = 0;
	for (int32_t i = 0; i < SQRT_BUILDING_COUNT; i++)
	{
		for (int32_t k = 0; k < SQRT_BUILDING_COUNT; k++)
		{
			float x, y, z;
			float size;

			x = float(i) / (float)SQRT_BUILDING_COUNT - 0.5f;
			y = 0.0f;
			z = float(k) / (float)SQRT_BUILDING_COUNT - 0.5f;
			size = .025f * (100.0f / (float)SQRT_BUILDING_COUNT);

			createBuilding(m_meshes[meshIndex + 1], ci::vec3(5.0f * x, y, 5.0f * z),
				ci::vec3(size, 0.2f + .1f * sin(5.0f * (float)(i * k)), size), ci::vec2(float(k) / (float)SQRT_BUILDING_COUNT, float(i) / (float)SQRT_BUILDING_COUNT));
			//ci::gl::VertBatchRef vBatch = ci::gl::VertBatch::create(GL_TRIANGLES, false);
			//m_vbo_meshes[meshIndex + 1] = ci::gl::VboMesh::create( ci::geom::Cube().size(ci::vec3(size, 0.2f + .1f * sin(5.0f * (float)(i * k)), size)) );
			//m_vbo_meshes[meshIndex + 1]->getVertexArrayVbos()[0]->
			meshIndex++;
		}
	}

	// Initialize Bindless Textures
	InitBindlessTextures();

	// create Uniform Buffer Object (UBO) for transform data and initialize 
	glGenBuffers(1, &m_transformUniforms);
	glNamedBufferDataEXT(m_transformUniforms, sizeof(TransformUniforms), &m_transformUniforms, GL_STREAM_DRAW);

	// create Uniform Buffer Object (UBO) for param data and initialize
	glGenBuffers(1, &m_perMeshUniforms);

	// Initialize the per mesh Uniforms
	updatePerMeshUniforms(0.0f);

	//CHECK_GL_ERROR();
}


void BindlessApp::InitBindlessTextures()
{
	char fileName[64] = { "textures/NV" };
	char Num[16];
	int i;

	m_textureHandles = new GLuint64[TEXTURE_FRAME_COUNT];
	m_textureIds = new GLuint[TEXTURE_FRAME_COUNT];
	m_numTextures = TEXTURE_FRAME_COUNT;

	for (i = 0; i < TEXTURE_FRAME_COUNT; ++i) {
		sprintf(Num, "%d", i);
		fileName[9] = 'N';
		fileName[10] = 'V';
		fileName[11] = 0;
		strcat(fileName, Num);
		strcat(fileName, ".dds");

		try {
			m_textureRefs[i] = ci::gl::Texture2d::createFromDds(ci::app::loadAsset(ci::fs::path(fileName)), gl::Texture2d::Format().internalFormat( GL_RGBA ).wrapS(GL_REPEAT).wrapT(GL_REPEAT).magFilter(GL_NEAREST).minFilter(GL_NEAREST).mipmap(false));
		}
		catch (ci::Exception& e) {
			//CI_LOG_EXCEPTION("failed to create texture from DDS file", e);
			ci::app::console() << "failed to create texture from DDS file" << &e << std::endl;
			quit();
		}
		m_textureIds[i] = m_textureRefs[i]->getId();
		m_textureRefs[i]->bind(m_textureIds[i]);

		m_textureHandles[i] = glGetTextureHandleNV(m_textureIds[i]);
		glMakeTextureHandleResidentNV(m_textureHandles[i]);
	}
}

void BindlessApp::mouseUp(MouseEvent event)
{
	mCamUI.mouseUp(event);
}
void BindlessApp::mouseDown(MouseEvent event)
{
	mCamUI.mouseDown(event);
}
void BindlessApp::mouseDrag(MouseEvent event)
{
	mCamUI.mouseDrag(event);
}
void BindlessApp::mouseWheel(MouseEvent event)
{
	mCamUI.mouseWheel(event);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::updatePerMeshUniforms()
//
//    Computes per mesh uniforms based on t and sends the uniforms to the GPU
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::updatePerMeshUniforms(float t)
{
	// If we're using per mesh uniforms, compute the values for the uniforms for all of the meshes and
	// give the data to the GPU.
	if (m_usePerMeshUniforms == true)
	{
		// Update uniforms for the "ground" mesh
		m_perMeshUniformsData[0].r = 1.0f;
		m_perMeshUniformsData[0].g = 1.0f;
		m_perMeshUniformsData[0].b = 1.0f;
		m_perMeshUniformsData[0].a = 0.0f;

		// Compute the per mesh uniforms for all of the "building" meshes
		int32_t index = 1;
		for (int32_t i = 0; i < SQRT_BUILDING_COUNT; i++)
		{
			for (int32_t j = 0; j < SQRT_BUILDING_COUNT; j++, index++)
			{
				float x, z, radius;

				x = float(i) / float(SQRT_BUILDING_COUNT) - 0.5f;
				z = float(j) / float(SQRT_BUILDING_COUNT) - 0.5f;
				radius = sqrt((x * x) + (z * z));

				m_perMeshUniformsData[index].r = sin(-4.f*10.0f * radius + t);
				m_perMeshUniformsData[index].g = cos(-4.f*10.0f * radius + t);
				m_perMeshUniformsData[index].b = radius;
				m_perMeshUniformsData[index].a = 0.0f;
				m_perMeshUniformsData[index].u = float(j) / float(SQRT_BUILDING_COUNT);
				m_perMeshUniformsData[index].v = float(i) / float(SQRT_BUILDING_COUNT);
			}
		}

		// Give the uniform data to the GPU
		glNamedBufferDataEXT(m_perMeshUniforms, m_perMeshUniformsData.size() * sizeof(m_perMeshUniformsData[0]), &(m_perMeshUniformsData[0]), GL_STREAM_DRAW);
	}
	else
	{
		// All meshes will use these uniforms
		m_perMeshUniformsData[0].r = sin(t);
		m_perMeshUniformsData[0].g = cos(t);
		m_perMeshUniformsData[0].b = 1.0f;
		m_perMeshUniformsData[0].a = 0.0f;

		// Give the uniform data to the GPU
		glNamedBufferSubDataEXT(m_perMeshUniforms, 0, sizeof(m_perMeshUniformsData[0]), &(m_perMeshUniformsData[0]));
	}



	if (m_useBindlessUniforms == true)
	{
		// *** INTERESTING ***
		// Get the GPU pointer for the per mesh uniform buffer and make the buffer resident on the GPU
		// For bindless uniforms, this GPU pointer will later be passed to the vertex shader via a
		// vertex attribute. The vertex shader will then directly use the GPU pointer to access the
		// uniform data.
		int32_t perMeshUniformsDataSize;
		glBindBuffer(GL_UNIFORM_BUFFER, m_perMeshUniforms);
		glGetBufferParameterui64vNV(GL_UNIFORM_BUFFER, GL_BUFFER_GPU_ADDRESS_NV, &m_perMeshUniformsGPUPtr);
		glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &perMeshUniformsDataSize);
		glMakeBufferResidentNV(GL_UNIFORM_BUFFER, GL_READ_ONLY);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
}


void BindlessApp::update()
{
	avgfps = getAverageFps();
	// Update the rendering stats in the UI
	float drawCallsPerSecond;
	drawCallsPerSecond = (float)m_meshes.size() * getAverageFps() * (float)Mesh::m_drawCallsPerState;
	m_drawCallsPerSecondText = drawCallsPerSecond / 1.0e6f;
	//m_drawCallsPerSecondText->SetValue(drawCallsPerSecond / 1.0e6f);

	m_currentTime += getElapsedSeconds();
	if (m_currentTime > ANIMATION_DURATION) m_currentTime = 0.0;
	m_currentFrame = (int)(180.0f*m_currentTime / ANIMATION_DURATION);

	gl::clear();//cleared in update

#ifdef USE_IMGUI
	{
		ui::ScopedWindow ui_sc_win("BindlessApp");
		{
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, Mesh::m_enableVBUM ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));

				ui::Text(("avg fps: " + ci::toString(getAverageFps())).c_str());
				ui::Text((ci::toString(m_drawCallsPerSecondText) + " M draw calls/sec").c_str());

			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, Mesh::m_enableVBUM ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Use bindless vertices/indices"))Mesh::m_enableVBUM = !Mesh::m_enableVBUM;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, m_useBindlessUniforms ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Use bindless uniforms"))m_useBindlessUniforms = !m_useBindlessUniforms;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, m_usePerMeshUniforms ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Use per mesh uniforms"))m_usePerMeshUniforms = !m_usePerMeshUniforms;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, m_updateUniformsEveryFrame ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Update uniforms every frame"))m_updateUniformsEveryFrame = !m_updateUniformsEveryFrame;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, Mesh::m_setVertexFormatOnEveryDrawCall ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Set vertex format for each mesh"))Mesh::m_setVertexFormatOnEveryDrawCall = !Mesh::m_setVertexFormatOnEveryDrawCall;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, Mesh::m_useHeavyVertexFormat ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Use heavy vertex format"))Mesh::m_useHeavyVertexFormat = !Mesh::m_useHeavyVertexFormat;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, m_useBindlessTextures ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::Button("Use bindless textures"))m_useBindlessTextures = !m_useBindlessTextures;
			}
			{
				ui::ScopedStyleColor scTcol(ImGuiCol_Text, Mesh::m_drawCallsPerState ? ImVec4(1., 1., 1., 1.) : ImVec4(0.90f, 0.00f, 0.90f, 1.00f));
				if (ui::DragInt("Draw calls per state", (int*)&Mesh::m_drawCallsPerState, 1., 1, 20));
			}

		}

	}
#else
	//draw Cinder native params instead
	mParams->draw();
#endif //USE_IMGUI

}

////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::draw()
//
//    Performs the actual rendering
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::draw()
{
	glm::mat4 modelviewMatrix;
	gl::ScopedMatrices scM;
	//gl::ScopedModelMatrix scMM;
	//gl::clear(Color(0, 0, 0));
	//gl::clear();//cleared in update
	//glEnable(GL_DEPTH_TEST);
	gl::ScopedDepth scDep(true);

	gl::setMatrices(mCam);
	// Enable the vertex and pixel shader
	//m_shader->enable();
	{
		gl::ScopedGlslProg scGl(m_shader);

		if (m_useBindlessTextures) {
			GLuint samplersLocation(m_shader->getUniformLocation("samplers"));
			glUniform1ui64vNV(samplersLocation, m_numTextures, m_textureHandles);

		}

		GLuint bBindlessTexture(m_shader->getUniformLocation("useBindless"));
		glUniform1i(bBindlessTexture, m_useBindlessTextures);

		GLuint currentTexture(m_shader->getUniformLocation("currentFrame"));
		glUniform1i(currentTexture, m_currentFrame);

		// Set the transformation matices up
		modelviewMatrix = ci::gl::getModelView();//m_transformer->getModelViewMat();
		m_projectionMatrix = mCam.getProjectionMatrix();
		m_transformUniformsData.ModelView = modelviewMatrix;
		m_transformUniformsData.ModelViewProjection = m_projectionMatrix * modelviewMatrix;
		m_transformUniformsData.UseBindlessUniforms = m_useBindlessUniforms;
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_transformUniforms);
		glNamedBufferSubDataEXT(m_transformUniforms, 0, sizeof(TransformUniforms), &m_transformUniformsData);


		// If we are going to update the uniforms every frame, do it now
		if (m_updateUniformsEveryFrame == true)
		{
			float deltaTime;
			float dt;

			deltaTime = getElapsedSeconds();

			if (deltaTime < m_minimumFrameDeltaTime)
			{
				m_minimumFrameDeltaTime = deltaTime;
			}

			dt = std::min(0.00005f / m_minimumFrameDeltaTime, .01f);
			m_t += dt * (float)Mesh::m_drawCallsPerState;

			updatePerMeshUniforms(m_t);
		}


		// Set up default per mesh uniforms. These may be changed on a per mesh basis in the rendering loop below 
		if (m_useBindlessUniforms == true)
		{
			// *** INTERESTING ***
			// Pass a GPU pointer to the vertex shader for the per mesh uniform data via a vertex attribute
			glVertexAttribI2i(m_bindlessPerMeshUniformsPtrAttribLocation,
				(int)(m_perMeshUniformsGPUPtr & 0xFFFFFFFF),
				(int)((m_perMeshUniformsGPUPtr >> 32) & 0xFFFFFFFF));
		}
		else
		{
			glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_perMeshUniforms);
			glNamedBufferSubDataEXT(m_perMeshUniforms, 0, sizeof(m_perMeshUniformsData[0]), &(m_perMeshUniformsData[0]));
		}

		// If all of the meshes are sharing the same vertex format, we can just set the vertex format once
		if (Mesh::m_setVertexFormatOnEveryDrawCall == false)
		{
			Mesh::renderPrep();
		}

		// Render all of the meshes
		for (int32_t i = 0; i < (int32_t)m_meshes.size(); i++)
		{
			// If enabled, update the per mesh uniforms for each mesh rendered
			if (m_usePerMeshUniforms == true)
			{
				if (m_useBindlessUniforms == true)
				{
					GLuint64EXT perMeshUniformsGPUPtr;

					// *** INTERESTING ***
					// Compute a GPU pointer for the per mesh uniforms for this mesh
					perMeshUniformsGPUPtr = m_perMeshUniformsGPUPtr + sizeof(m_perMeshUniformsData[0]) * i;
					// Pass a GPU pointer to the vertex shader for the per mesh uniform data via a vertex attribute
					glVertexAttribI2i(m_bindlessPerMeshUniformsPtrAttribLocation,
						(int)(perMeshUniformsGPUPtr & 0xFFFFFFFF),
						(int)((perMeshUniformsGPUPtr >> 32) & 0xFFFFFFFF));
				}
				else
				{
					glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_perMeshUniforms);
					glNamedBufferSubDataEXT(m_perMeshUniforms, 0, sizeof(m_perMeshUniformsData[0]), &(m_perMeshUniformsData[i]));
				}
			}

			// If we're not sharing vertex formats between meshes, we have to set the vertex format everytime it changes.
			if (Mesh::m_setVertexFormatOnEveryDrawCall == true)
			{
				Mesh::renderPrep();
			}

			// Now that everything is set up, do the actual rendering
			// The code that selects between rendering with Vertex Array Objects (VAO) and 
			// Vertex Buffer Unified Memory (VBUM) is located in Mesh::render()
			// The code that gets the GPU pointer for use with VBUM rendering is located in Mesh::update()
			m_meshes[i].render();


			// If we're not sharing vertex formats between meshes, we have to reset the vertex format to a default state after each mesh
			if (Mesh::m_setVertexFormatOnEveryDrawCall == true)
			{
				Mesh::renderFinish();
			}
		}

		// If we're sharing vertex formats between meshes, we only have to reset vertex format to a default state once
		if (Mesh::m_setVertexFormatOnEveryDrawCall == false)
		{
			Mesh::renderFinish();
		}

		// Disable the vertex and pixel shader
		//m_shader->disable();
	}

}

////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::createGround()
//
//    Create a mesh for the ground
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::createGround(Mesh& mesh, ci::vec3 pos, ci::vec3 dim)
{
	std::vector<Vertex>         vertices;
	std::vector<uint16_t> indices;
	float                  r, g, b;

	dim.x *= 0.5f;
	dim.z *= 0.5f;

	// +Y face
	r = 0.3f; g = 0.3f; b = 0.3f;
	vertices.push_back(Vertex(-dim.x + pos.x, pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, pos.y, -dim.z + pos.z, r, g, b, 1.0f));

	// Create the indices
	indices.push_back(0); indices.push_back(1); indices.push_back(2);
	indices.push_back(0); indices.push_back(2); indices.push_back(3);

	mesh.update(vertices, indices);
}

////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::createBuilding()
//
//    Create a very simple building mesh
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::createBuilding(Mesh& mesh, ci::vec3 pos, ci::vec3 dim, ci::vec2 uv)
{
	std::vector<Vertex>         vertices;
	std::vector<uint16_t> indices;
	float                  r, g, b;

	dim.x *= 0.5f;
	dim.z *= 0.5f;

	// Generate a simple building model (i.e. a box). All of the "buildings" are in world space

	// +Z face
	r = 0.0f; g = 0.0f; b = 1.0f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));

	// -Z face
	r = 0.0f; g = 0.0f; b = 0.5f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));

	// +X face
	r = 1.0f; g = 0.0f; b = 0.0f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));

	// -X face
	r = 0.5f; g = 0.0f; b = 0.0f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));

	// +Y face
	r = 0.0f; g = 1.0f; b = 0.0f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, dim.y + pos.y, -dim.z + pos.z, r, g, b, 1.0f));

	// -Y face
	r = 0.0f; g = 0.5f; b = 0.0f;
	randomColor(r, g, b);
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, -dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(+dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));
	vertices.push_back(Vertex(-dim.x + pos.x, 0.0f + pos.y, +dim.z + pos.z, r, g, b, 1.0f));


	// Create the indices
	for (int32_t i = 0; i < 24; i += 4)
	{
		indices.push_back((uint16_t)(0 + i));
		indices.push_back((uint16_t)(1 + i));
		indices.push_back((uint16_t)(2 + i));

		indices.push_back((uint16_t)(0 + i));
		indices.push_back((uint16_t)(2 + i));
		indices.push_back((uint16_t)(3 + i));
	}


	mesh.update(vertices, indices);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Method: BindlessApp::randomColor()
//
//    Generates a random color
//
////////////////////////////////////////////////////////////////////////////////
void BindlessApp::randomColor(float &r, float &g, float &b)
{
	r = float(rand() % 255) / 255.0f;
	g = float(rand() % 255) / 255.0f;
	b = float(rand() % 255) / 255.0f;
}

void BindlessApp::resize()
{
	mCam.setAspectRatio(getWindowAspectRatio());

}


auto settingsFunc = [](App::Settings *settings)
{
	settings->setHighDensityDisplayEnabled();
	settings->prepareWindow(Window::Format().size(vec2(1280, 720)).title("NV_Compute_Particles"));
#ifdef DISABLE_FRAMERATE
	settings->disableFrameRate();
#else
	settings->setFrameRate(60.0f);
#endif //DISABLE_FRAMERATE
	//#if defined( CINDER_MSW )//#endif
	//settings->setConsoleWindowEnabled();
};


CINDER_APP(BindlessApp, RendererGl(ci::app::RendererGl::Options().version(4, 6).msaa(16)), settingsFunc)
