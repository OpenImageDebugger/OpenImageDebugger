from conan import ConanFile

class MyProjectConan(ConanFile):
    name = "myproject"
    version = "0.1"
    settings = "os", "arch", "compiler", "build_type"
    requires = "protobuf/3.21.12"
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "CMakeLists.txt", "main.cpp"
