#include <array>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <GL/glew.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <IceTGL.h>
#include <IceTGL3.h>
#include <IceTMPI.h>
#include <mpi.h>

#if ICETDEMO_PLATFORM_INTERFACE == 0 // EGL
	#include <EGL/egl.h>
	#include <png.hpp>
#elif ICETDEMO_PLATFORM_INTERFACE == 1 // glfw
	#include <GLFW/glfw3.h>
	#endif

#include "buildinfo.hpp"


namespace {

// Aliases.
namespace fs = std::filesystem;


// Output utilities.
std::string_view constexpr log_sev_info  {      "\x1b[1m[info]\x1b[m  "};
std::string_view constexpr log_sev_warn  {   "\x1b[1;33m[warn]\x1b[m  "};
std::string_view constexpr log_sev_error {   "\x1b[1;31m[error]\x1b[m "};
std::string_view constexpr log_sev_fatal {"\x1b[1;30;41m[fatal]\x1b[m "};

std::string_view constexpr log_tag_egl    {"\x1b[1m[egl]\x1b[m "};
std::string_view constexpr log_tag_glfw   {"\x1b[1m[glfw]\x1b[m "};
std::string_view constexpr log_tag_opengl {"\x1b[1m[opengl]\x1b[m "};

/// Concatenate arguments into a string using a string stream for formatting.
template<typename... TArgs>
auto concat(TArgs const&... args) -> std::string {
	std::ostringstream result;
	(result << ... << args);
	return std::move(result).str();
	}


/// Base class for an RAII wrapper that uniquely owns a handle.
template<std::regular THandle, std::regular_invocable<THandle&&> TDeleter>
	requires std::default_initializable<TDeleter>
class Handle {
public:
	using RawHandle = THandle;


	[[nodiscard]] constexpr Handle() noexcept = default;

	Handle(Handle const&) = delete;

	constexpr Handle(Handle&& src) noexcept
		: _handle {std::move(src._handle)}
		{
		src._handle = {};
		}

	auto operator=(Handle const&) = delete;

	constexpr auto operator=(Handle&& src) noexcept -> Handle&
		{
		if (_handle == src._handle) {
			return *this;
			}

		this->~Handle();
		_handle     = src._handle;
		src._handle = {};
		return *this;
		}

	~Handle() noexcept {
		if (_handle != THandle{}) {
			TDeleter{}(std::move(_handle));
			_handle = {};
			}}


	[[nodiscard]] auto constexpr operator==(Handle const& rhs) const noexcept -> bool {
		return _handle = rhs._handle;
		}

	/// Return the raw handle.
	[[nodiscard]] constexpr auto handle() const noexcept -> THandle const& {
		return _handle;
		}

protected:
	THandle _handle {};

	[[nodiscard]] constexpr explicit Handle(THandle&& handle) noexcept
		: _handle {std::move(handle)}
		{}

	};


/// Convenience wrappers for MPI.
namespace mpi {

/// Return the message associated with an MPI error code.
auto error_message(int const error_code) noexcept -> std::string {
	// Allocate memory to hold the message.
	std::string msg (MPI_MAX_ERROR_STRING, '\0');
	int length;

	// Retrieve the message.
	if (MPI_Error_string(error_code, msg.data(), &length)) {
		// If no message could be found, return the error code directly.
		return std::to_string(error_code);
		}

	// Trim excess characters.
	msg.resize(length);

	return msg;
	}


/// An RAII handle to the MPI execution environment.
struct Runtime {
	/// MPI initialization.
	[[nodiscard]] Runtime(int* argc = nullptr, char*** argv = nullptr) {
		if (auto const error = MPI_Init(argc, argv)) {
			throw std::runtime_error(concat("Could not initialize MPI: ", error_message(error)));
			}}

	Runtime(Runtime const&) = delete;
	Runtime(Runtime&&) = delete;

	auto operator=(Runtime const&) = delete;
	auto operator=(Runtime&&) = delete;

	/// MPI cleanup,
	~Runtime() noexcept {
		MPI_Finalize();
		}

	};

} // namespace mpi


/// Convenience wrappers for IceT.
namespace icet {

/// RAII handle for an `IceTCommunicator`.
class Communicator : public Handle<
		IceTCommunicator,
		decltype([](IceTCommunicator&& com) {icetDestroyMPICommunicator(std::move(com));})
		> {
public:
	[[nodiscard]] explicit Communicator(MPI_Comm const& mpi_com) noexcept
		: Handle{icetCreateMPICommunicator(mpi_com)}
		{}

	};

/// RAII handle for an `IceTContext`.
class Context : public Handle<
		IceTContext,
		decltype([](IceTContext&& ctx) {icetDestroyContext(std::move(ctx));})
		> {
public:
	[[nodiscard]] explicit Context(Communicator const& com) noexcept
		: Handle{icetCreateContext(com.handle())}
		{}

	};

} // namespace icet


/// Convenience wrappers for OpenGL.
namespace gl {

/// Print a debug message to stderr.
auto GLAPIENTRY print_error(
		GLenum const        src,
		GLenum const        type,
		GLuint const        id,
		GLenum const        severity,
		GLsizei const       length,
		GLchar const* const msg,
		void const* const   user_data
		) -> void {
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			std::cerr << log_sev_error;
			break;
		default:
			std::cerr << log_sev_warn;
		}

	std::cerr << log_tag_opengl << msg << "\n";
	}

/// RAII handle for a Vertex Array Object.
class VertexArray : public Handle<
		GLuint,
		decltype([](GLuint handle) {glDeleteVertexArrays(1, &handle);})
		> {
public:
	[[nodiscard]] VertexArray() noexcept {
		glGenVertexArrays(1, &_handle);
		}

	};

/// RAII handle for a Buffer Object.
class Buffer : public Handle<GLuint, decltype([](GLuint handle) {glDeleteBuffers(1, &handle);})> {
public:
	[[nodiscard]] Buffer() noexcept {
		glGenBuffers(1, &_handle);
		}

	};

/// RAII handle for a Shader Object.
class ShaderStage : public Handle<GLuint, decltype([](GLuint handle) {glDeleteShader(handle);})> {
public:
	[[nodiscard]] ShaderStage(GLenum const type, std::string_view const src) noexcept
		: Handle{glCreateShader(type)}
		{
		compile(src);
		}

	auto compile(std::string_view src) noexcept -> void {
		// Pass the source code to OpenGL.
		auto* src_ptr {src.data()};
		GLint src_len {static_cast<GLint>(src.size())};
		glShaderSource(_handle, 1, &src_ptr, &src_len);

		// Compile the shader stage.
		glCompileShader(_handle);
		}

	};

/// RAII handle for a Program Object.
class ShaderProgram : public Handle<
		GLuint,
		decltype([](GLuint handle) {glDeleteProgram(handle);})
		> {
public:
	[[nodiscard]] ShaderProgram() noexcept
		: Handle{glCreateProgram()}
		{}

	auto link(std::vector<ShaderStage> const& stages) -> void {
		// Attach stages.
		for (auto const& stage : stages) {
			glAttachShader(_handle, stage.handle());
			}

		// Link.
		glLinkProgram(_handle);

		// Detach stages.
		for (auto const& stage : stages) {
			glDetachShader(_handle, stage.handle());
			}}

	};

/// RAII handle for a Texture Object.
class Texture : public Handle<GLuint, decltype([](GLuint handle) {glDeleteTextures(1, &handle);})> {
public:
	[[nodiscard]] Texture() noexcept {
		glGenTextures(1, &_handle);
		}

	};

/// RAII handle for a Framebuffer Object.
class Framebuffer : public Handle<
		GLuint,
		decltype([](GLuint handle) {glDeleteFramebuffers(1, &handle);})
		> {
public:
	[[nodiscard]] Framebuffer() noexcept {
		glGenFramebuffers(1, &_handle);
		}

	};

} // namespace gl


#if ICETDEMO_PLATFORM_INTERFACE == 0 // EGL

/// Convenience wrappers for EGL.
namespace egl {

/// Return the identifier corresponding to an EGL error code.
[[nodiscard]] auto constexpr error_name(EGLint error_code) -> std::string_view {
	switch (error_code) {
		case EGL_SUCCESS:             return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
		default:                      return "unknown error code";
		}}

/// An RAII handle to an EGL display connection.
class Display : public Handle<
		EGLDisplay,
		decltype([](EGLDisplay&& handle) {
			if (not eglTerminate(handle)) {
				std::clog << log_tag_egl
				          << "Failed to release display: "
				          << error_name(eglGetError())
				          << "\n";
				}})
		> {
public:
	[[nodiscard]] explicit Display(EGLDisplay&& display)
		: Handle{std::move(display)}
		{
		if (not eglInitialize(display, nullptr, nullptr)) {
			throw std::runtime_error(concat(
					log_tag_egl,
					"Failed to create display connection: ",
					error_name(eglGetError())
					));
			}}

	};

/// An RAII handle to an EGL surface.
class Surface : public Handle<
		std::tuple<EGLDisplay, EGLSurface>,
		decltype([](std::tuple<EGLDisplay, EGLSurface>&& handles) {
			if (not eglDestroySurface(std::get<0>(handles), std::get<1>(handles))) {
				std::clog << log_sev_error
				          << log_tag_egl
				          << "Failed to destroy surface: "
				          << error_name(eglGetError())
				          << "\n";
				}})
		> {
public:
	using RawHandle = EGLSurface;

	[[nodiscard]] static Surface create_pbuffer(
			Display const& display,
			EGLConfig      config,
			EGLint const*  attribs
			) {
		auto const surface {eglCreatePbufferSurface(display.handle(), config, attribs)};

		if (not surface) {
			throw std::runtime_error(concat(
					log_tag_egl, "Failed to create pixel buffer: ", error_name(eglGetError())));
			}

		return {display.handle(), surface};
		}

	[[nodiscard]] constexpr auto handle() const noexcept -> EGLSurface const& {
		return std::get<1>(_handle);
		}

private:
	[[nodiscard]] constexpr Surface(EGLDisplay display, EGLSurface handle) noexcept
		: Handle{{display, handle}}
		{}

	};

/// An RAII handle to an EGL context.
class Context : public Handle<
		std::tuple<EGLDisplay, EGLContext>,
		decltype([](std::tuple<EGLDisplay, EGLContext>&& handles) {
			if (not eglDestroyContext(std::get<0>(handles), std::get<1>(handles))) {
				std::cerr << log_sev_error
				          << log_tag_egl
				          << "Failed to destroy context: "
				          << error_name(eglGetError());
				}})
		> {
public:
	using RawHandle = EGLContext;

	[[nodiscard]] Context(
			Display const& display,
			EGLConfig      config,
			EGLint const*  attribs        = nullptr,
			EGLContext     shared_context = EGL_NO_CONTEXT
			)
		: Handle{{
			display.handle(),
			eglCreateContext(display.handle(), config, shared_context, attribs)
			}}
		{
		if (not std::get<1>(_handle)) {
			throw std::runtime_error(concat(
					log_tag_egl, "Failed to create context: ", error_name(eglGetError())));
			}}

	[[nodiscard]] constexpr auto handle() const noexcept -> EGLContext const& {
		return std::get<1>(_handle);
		}

	};

} // namespace egl

#elif ICETDEMO_PLATFORM_INTERFACE == 1 // glfw

/// Convenience wrappers for glfw3.
namespace glfw {

/// Callback that prints an error message to stderr.
auto print_error(int const error_code, char const* error_message) {
	std::cerr << log_sev_error << log_tag_glfw << error_message << "\n";
	}

/// An RAII object managing initialization and cleanup.
struct Library {
	[[nodiscard]] Library() {
		if (not glfwInit()) {
			throw std::runtime_error{"Failed to initialize GLFW."};
			}}

	~Library() noexcept {
		glfwTerminate();
		}

	};

/// An RAII handle for a window.
class Window : public Handle<
		GLFWwindow*,
		decltype([](GLFWwindow* handle) {glfwDestroyWindow(handle);})
		> {
public:
	[[nodiscard]] Window(
			int const          width,
			int const          height,
			std::string const& title,
			GLFWmonitor*       monitor        = nullptr,
			GLFWwindow*        shared_context = nullptr
			)
		: Handle{glfwCreateWindow(width, height, title.c_str(), monitor, shared_context)}
		{}

	};

}

#endif // ICETDEMO_PLATFORM_INTERFACE


/// Read the entire content of a file into a string.
auto read_file(fs::path const& path) -> std::string {
	std::ifstream file {path};
	std::string content;

	content.reserve(fs::file_size(path));
	content.assign(std::istreambuf_iterator<char>(file), {});

	return content;
	}


/// Custom rendering logic.
class QuadRenderer {
public:
	struct Quad {
		glm::vec3 center;
		};

	[[nodiscard]] explicit QuadRenderer(glm::vec3 const& color) noexcept {
		// Objects are implicitely constructed.

		// Bind vertex array and buffer.
		glBindVertexArray(vao.handle());
		glBindVertexBuffer(0, quads.handle(), 0, sizeof(Quad));
		glBindBuffer(GL_ARRAY_BUFFER, quads.handle());

		// Build and use shader program.
		std::vector<gl::ShaderStage> stages;
		stages.reserve(3);
		stages.emplace_back(GL_VERTEX_SHADER,   read_file(buildinfo::resource_dir / "shaders/demo.vert"));
		stages.emplace_back(GL_GEOMETRY_SHADER, read_file(buildinfo::resource_dir / "shaders/demo.geom"));
		stages.emplace_back(GL_FRAGMENT_SHADER, read_file(buildinfo::resource_dir / "shaders/demo.frag"));
		shader.link(stages);
		glUseProgram(shader.handle());

		// Configure vertex attributes.
		auto loc = glGetAttribLocation(shader.handle(), "center");
		glEnableVertexAttribArray(loc);
		glVertexAttribFormat(loc, 3, GL_FLOAT, GL_FALSE, offsetof(Quad, center));
		glVertexAttribBinding(loc, 0);

		// Get uniform locations.
		ufm_mvp_mat = glGetUniformLocation(shader.handle(), "mvp_mat");

		// Set uniforms.
		glUniform3fv(glGetUniformLocation(shader.handle(), "color"), 1, glm::value_ptr(color));
		}

	auto upload(std::vector<Quad> const& quads) noexcept -> void {
		glBufferData(GL_ARRAY_BUFFER, quads.size() * sizeof(Quad), quads.data(), GL_STATIC_DRAW);
		num_quads = quads.size();
		}

	auto draw(glm::mat4 const& mvp_mat, GLuint fbo) noexcept -> void {
		glBindVertexArray(vao.handle());
		glUseProgram(shader.handle());

		glUniformMatrix4fv(ufm_mvp_mat, 1, false, glm::value_ptr(mvp_mat));

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDrawArrays(GL_POINTS, 0, num_quads);

		local_fbo = fbo;
		}

private:
	GLsizeiptr num_quads     {0};
	gl::VertexArray vao      {};
	gl::Buffer quads         {};
	gl::ShaderProgram shader {};
	GLint ufm_mvp_mat        {-1};
	GLuint local_fbo         {0};
	};


/// Pointer to the renderer instance used by this process.
/// Must be global so it can be accessed by the IceT draw callback.
QuadRenderer* renderer {nullptr};

/// Callback invoked by IceT.
auto draw(
		GLdouble const* p_mat,
		GLdouble const* mv_mat,
		GLint const*    viewport,
		GLuint          fbo
		) -> void {
	renderer->draw(glm::make_mat4(p_mat) * glm::make_mat4(mv_mat), fbo);
	}

} // namespace


auto main(int argc, char** argv) -> int try {
	using namespace std::string_literals;

	// Application configuration.
	int  width     {750};
	int  height    {750};
	auto num_quads {100};

	switch (argc) {
		default:
			num_quads = atoi(argv[3]);
		case 3:
			height = atoi(argv[2]);
		case 2:
			width = atoi(argv[1]);
		case 1:
		case 0:
			;
		}

	// Initialize MPI and retrieve configuration.
	mpi::Runtime mpi {&argc, &argv};
	auto const mpi_com {MPI_COMM_WORLD};

	int proc_rank;
	MPI_Comm_rank(mpi_com, &proc_rank);

	int num_procs;
	MPI_Comm_size(mpi_com, &num_procs);

	if (proc_rank == 0) {
		std::clog << log_sev_info << "Using " << num_procs << " processes.\n";
		}

	// Initialize and configure IceT.
	icet::Communicator com {mpi_com};
	icet::Context ctx      {com};

	icetSetColorFormat(ICET_IMAGE_COLOR_RGBA_UBYTE);
	icetSetDepthFormat(ICET_IMAGE_DEPTH_FLOAT);

	// Create surface and OpenGL context.
	#if ICETDEMO_PLATFORM_INTERFACE == 0 // EGL
		// Based on https://developer.nvidia.com/blog/egl-eye-opengl-visualization-without-x-server.

		// Display.
		egl::Display const display {eglGetDisplay(EGL_DEFAULT_DISPLAY)};

		// Config.
		EGLint constexpr config_attribs[] {
			EGL_CONFORMANT,        EGL_OPENGL_BIT,
			EGL_SURFACE_TYPE,      EGL_PBUFFER_BIT,
			EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
			EGL_RED_SIZE,          8,
			EGL_GREEN_SIZE,        8,
			EGL_BLUE_SIZE,         8,
			EGL_ALPHA_SIZE,        8,
			EGL_DEPTH_SIZE,        8,
			EGL_NONE
			};
		EGLConfig config      {};
		EGLint    num_configs {0};
		eglChooseConfig(display.handle(), config_attribs, &config, 1, &num_configs);

		// Surface (pbuffer).
		EGLint const surface_attribs[] {
			EGL_WIDTH,  width,
			EGL_HEIGHT, height,
			EGL_NONE
			};
		auto const surface {egl::Surface::create_pbuffer(display, config, surface_attribs)};

		// OpenGL context.
		if (not eglBindAPI(EGL_OPENGL_API)) {
			throw std::runtime_error(concat(
					log_tag_egl, "Failed to bind OpenGL: ", egl::error_name(eglGetError())));
			}

		egl::Context const context {display, config};
		eglMakeCurrent(display.handle(), surface.handle(), surface.handle(), context.handle());

	#elif ICETDEMO_PLATFORM_INTERFACE == 1 // glfw
		// Create and configure window.
		glfw::Library const glfw {};
		glfwSetErrorCallback(glfw::print_error);

		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		glfw::Window window {
				width,
				height,
				concat("IceT Demo [", proc_rank + 1, '/', num_procs, ']')
				};

		glfwMakeContextCurrent(window.handle());
		#endif

	// Initialize and set up OpenGL.
	if (auto const error = glewInit()) {
		throw std::runtime_error(concat("Failed to initialize GLEW: ", glewGetErrorString(error)));
		}

	glDebugMessageCallback(gl::print_error, nullptr);
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageControl(
			GL_DONT_CARE,
			GL_DONT_CARE,
			GL_DEBUG_SEVERITY_NOTIFICATION,
			0,
			nullptr,
			GL_FALSE
			);

	glClearColor(.3, .2, .3, 1);
	glEnable(GL_DEPTH_TEST);

	// Set up rendering.
	auto const norm_rank {static_cast<float>(proc_rank) / num_procs};
	QuadRenderer renderer {{
			 3 * std::abs(norm_rank - 1./2) - .5,
			-3 * std::abs(norm_rank - 1./3) + 1,
			-3 * std::abs(norm_rank - 2./3) + 1
			}};
	::renderer = &renderer;

	{
	std::vector<QuadRenderer::Quad> quads;
	quads.reserve(num_quads / num_procs);

	using Rng = std::mt19937;
	Rng rng {static_cast<Rng::result_type>(proc_rank)};
	std::uniform_real_distribution<float> pos_dist {-1, 1};

	for (auto i {quads.size()}; i < quads.capacity(); ++i) {
		quads.push_back({{pos_dist(rng), pos_dist(rng), pos_dist(rng)}});
		}

	renderer.upload(quads);
	}

	// Set up IceT with OpenGL.
	icetGL3Initialize();
	icetGL3DrawCallbackTexture(draw);

	icetBoundingBoxf(-1, 1, -1, 1, -1, 1);
	icetStrategy(ICET_STRATEGY_SEQUENTIAL);
	icetSingleImageStrategy(ICET_SINGLE_IMAGE_STRATEGY_GPU_BSWAP);
	icetResetTiles();

	icetAddTile(0, 0, width, height, 0);

	// auto const tile_width {width / num_procs};

	// for (IceTInt rank {0}; rank < num_procs; ++rank) {
	// 	icetAddTile(rank * tile_width, 0, tile_width, height, rank);
	// 	}

	// Render.
	glm::dmat4 p_mat  {glm::ortho(-1, 1, -1, 1, -1, 1)};
	glm::dmat4 mv_mat {1};

	auto const image {icetGL3DrawFrame(glm::value_ptr(p_mat), glm::value_ptr(mv_mat))};

	// glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.local_fbo);
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	// glBlitFramebuffer(
	// 		0, 0, width, height,
	// 		0, 0, width, height,
	// 		GL_COLOR_BUFFER_BIT,
	// 		GL_NEAREST
	// 		);

	// Output tile.
	IceTInt local_tile {-1};
	icetGetIntegerv(ICET_TILE_DISPLAYED, &local_tile);

	if (local_tile == -1) {
		return EXIT_SUCCESS;
		}

	auto const img_width  = icetImageGetWidth(image);
	auto const img_height = icetImageGetHeight(image);
	std::clog << log_sev_info << "Rendered a " << width << "×" << img_height << " px image.\n";

	#if ICETDEMO_PLATFORM_INTERFACE == 0 // EGL
		struct ImageRows : png::generator<png::rgba_pixel, ImageRows> {
			IceTUByte* pixels;

			[[nodiscard]] ImageRows(IceTImage image, png::image_info& info)
				: generator(info)
				, pixels {icetImageGetColorub(image)}
				{}

			auto get_next_row(png::uint_32 row) -> png::byte* {
				return pixels + row * get_info().get_width() * 4;
				}

			};

		png::image_info info;
		info.set_width(img_width);
		info.set_height(img_height);
		info.set_bit_depth(8);
		info.set_color_type(png::color_type_rgba);

		std::ofstream out ("out.png");
		ImageRows{image, info}.write(out);

	#elif ICETDEMO_PLATFORM_INTERFACE == 1 // glfw
		// auto const tile_width {img_width / num_procs};

		gl::Texture const final_image {};
		glBindTexture(GL_TEXTURE_RECTANGLE, final_image.handle());
		glTexImage2D(
				GL_TEXTURE_RECTANGLE,
				0,
				GL_RGBA,
				img_width,
				img_height,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				icetImageGetColorub(image)
				);

		gl::Framebuffer const final_fbo {};
		glBindFramebuffer(GL_READ_FRAMEBUFFER, final_fbo.handle());
		glFramebufferTexture2D(
				GL_READ_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_RECTANGLE,
				final_image.handle(),
				0
				);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(
				0, 0, img_width, img_height,
				0, 0, img_width, img_height,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
				);

		glfwSwapBuffers(window.handle());

		// Main loop.
		while (not glfwWindowShouldClose(window.handle())) {
			glfwPollEvents();
			}

		#endif // ICETDEMO_PLATFORM_INTERFACE == glfw

	return EXIT_SUCCESS;
	}
	catch (std::exception const& error) {
		std::cerr << log_sev_fatal << error.what() << "\n";
		return EXIT_FAILURE;
		}
	catch (...) {
		std::cerr << log_sev_fatal << "Unknown error\n";
		return EXIT_FAILURE;
		}
