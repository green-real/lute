local task = require("@lute/task") :: any
local ffi = require("@lute/ffi") :: any
local c = ffi.c

local char = c.char
local int = c.int
local float = c.float

local glfw = require("./gl/glfw")
local gl = require("./gl/gl")

local function checkCompileErrors(shader: number, isProgram: boolean, type: string)
    local success = c.new(int)
    local infoLogLength = c.new(int)

    if isProgram then
        gl.GetProgramiv(shader, gl.LINK_STATUS, success:ptr())
        gl.GetProgramiv(shader, gl.INFO_LOG_LENGTH, infoLogLength:ptr())
    else
        gl.GetShaderiv(shader, gl.COMPILE_STATUS, success:ptr())
        gl.GetShaderiv(shader, gl.INFO_LOG_LENGTH, infoLogLength:ptr())
    end

    if success:read() == 0 then
        local str_t = c.array(char, infoLogLength:read() + 1)
        local infoLog = c.new(str_t)
        if isProgram then
            gl.GetProgramInfoLog(shader, infoLogLength, nil, infoLog)
            error(`Error linking {type}: {infoLog:string()}`)
        else
            gl.GetShaderInfoLog(shader, infoLogLength, nil, infoLog)
            error(`Error compiling {type}: {infoLog:string()}`)
        end
    end
end

local function createShader(shaderType: number, source: string): number
    local shader = gl.CreateShader(shaderType)
    if shader:read() == 0 then
        error("Failed to create shader")
    end

    local sourceLength = c.new(int, #source)
    gl.ShaderSource(shader, 1, c.string(source):ptr(), sourceLength:ptr())
    gl.CompileShader(shader)

    checkCompileErrors(shader, false, "shader")

    return shader
end

local function createProgram(vertexShader: number, fragmentShader: number): number
    local program = gl.CreateProgram()
    if program:read() == 0 then
        error("Failed to create shader program")
    end

    gl.AttachShader(program, vertexShader)
    gl.AttachShader(program, fragmentShader)
    gl.LinkProgram(program)

    checkCompileErrors(program, true, "program")

    gl.DeleteShader(vertexShader)
    gl.DeleteShader(fragmentShader)

    return program
end

local function main()
    if glfw.Init():read() == 0 then
        error("Failed to initialize GLFW")
    end

    glfw.WindowHint(glfw.CONTEXT_VERSION_MAJOR, 3) -- GLFW_CONTEXT_VERSION_MAJOR = 3
    glfw.WindowHint(glfw.CONTEXT_VERSION_MINOR, 3) -- GLFW_CONTEXT_VERSION_MINOR = 3
    glfw.WindowHint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE) -- GLFW_OPENGL_PROFILE = GLFW_OPENGL_CORE_PROFILE
    print("GLFW Version: " .. glfw.GetVersionString():string())

    local window = glfw.CreateWindow(800, 600, "Hi", nil, nil)
    if window:isnull() then
        glfw.Terminate()
        error("Failed to create GLFW window")
    end

    glfw.MakeContextCurrent(window)
    gl.Load(glfw.GetProcAddress)

    local vertexShader = createShader(gl.VERTEX_SHADER, [[
        #version 330 core
        layout(location = 0) in vec3 aPos;
        void main() {
            gl_Position = vec4(aPos, 1.0);
        }
    ]])

    local fragmentShader = createShader(gl.FRAGMENT_SHADER, [[
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 0.5, 0.2, 1.0);
        }
    ]])

    local shaderProgram = createProgram(vertexShader, fragmentShader)

    local VAO, VBO = c.new(int), c.new(int)
    gl.GenVertexArrays(1, VAO:ptr())
    gl.GenBuffers(1, VBO:ptr())

    local vertices = {
        0.0,  0.5, 0.0,
        -0.5, -0.5, 0.0,
        0.5, -0.5, 0.0
    }

    local vertexData = c.new(c.array(float, #vertices), vertices)

    gl.BindVertexArray(VAO)
    gl.BindBuffer(gl.ARRAY_BUFFER, VBO)
    gl.BufferData(gl.ARRAY_BUFFER, #vertices * c.sizeof(float), vertexData, gl.STATIC_DRAW)
    gl.VertexAttribPointer(0, 3, gl.FLOAT, false, 3 * c.sizeof(float), nil)
    gl.EnableVertexAttribArray(0)

    gl.ClearColor(0.1, 0.1, 0.1, 1.0)

    while glfw.WindowShouldClose(window):read() == 0 do
        gl.Clear(gl.COLOR_BUFFER_BIT)

        gl.UseProgram(shaderProgram)
        gl.BindVertexArray(VAO)
        gl.DrawArrays(gl.TRIANGLES, 0, 3)
        
        glfw.SwapBuffers(window)
        glfw.PollEvents()

        task.wait() -- yield to allow other tasks to run
    end

    gl.DeleteVertexArrays(1, VAO:ptr())
    gl.DeleteBuffers(1, VBO:ptr())
    gl.DeleteProgram(shaderProgram)

    glfw.DestroyWindow(window)
    glfw.Terminate()

    print("Exiting...")
end



main()
