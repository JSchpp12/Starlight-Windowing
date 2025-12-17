#include "star_windowing/StarWindow.hpp"

namespace star
{

void StarWindow::cleanupRender(){
    DestroyWindow(this->window);
}

void StarWindow::initWindowInfo()
{
    // need to give GLFW a pointer to current instance of this class
    glfwSetWindowUserPointer(this->window, this);

    // glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    // auto callback = glfwSetKeyCallback(this->window, InteractionSystem::glfwKeyHandle);
    // auto mouseButtonCallback = glfwSetMouseButtonCallback(this->window, InteractionSystem::glfwMouseButtonCallback);
    // auto cursorCallback = glfwSetCursorPosCallback(this->window, InteractionSystem::glfwMouseMovement);
    // auto mouseScrollCallback = glfwSetScrollCallback(this->window, InteractionSystem::glfwScrollCallback);
}

StarWindow::StarWindow(const int &width, const int &height, const std::string &title)
    : window(CreateGLFWWindow(width, height, title))
{
    initWindowInfo();
}

StarWindow::Builder &StarWindow::Builder::setWidth(const int &nWidth)
{
    this->width = nWidth;
    return *this;
}

StarWindow::Builder &StarWindow::Builder::setHeight(const int &nHeight)
{
    this->height = nHeight;
    return *this;
}

StarWindow::Builder &StarWindow::Builder::setTitle(const std::string &nTitle)
{
    this->title = nTitle;
    return *this;
}

star::StarWindow StarWindow::Builder::build(){
    return StarWindow(this->width, this->height, this->title);
}

std::unique_ptr<star::StarWindow> StarWindow::Builder::buildUnique()
{
    assert(this->width > 0 && this->height > 0 && "Width and height must be defined");

    return std::unique_ptr<StarWindow>(new StarWindow(this->width, this->height, this->title));
}

GLFWwindow *StarWindow::CreateGLFWWindow(const int &width, const int &height, const std::string &title)
{
    glfwInit();
    // tell GLFW to create a window but to not include a openGL instance as this is a default behavior
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // disable resizing functionality in glfw as this will not be handled in the first tutorial
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    return glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

void StarWindow::DestroyWindow(GLFWwindow *window){
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace star