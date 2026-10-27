# HMCL 与 JavaFX 图形加速

HMCL 启动器显示“软件渲染”时，给它加一个 Java 参数就能用上 GPU：`-Dprism.forceGPU=true`。启动器界面用的是 JavaFX，Minecraft 自己用 OpenGL，两者互不影响。

Azul Zulu 的 JavaFX 在 Linux 上有一份驱动白名单，里面没有 `freedreno`。即使 Mesa 已经提供 Adreno 硬件加速，JavaFX 仍会提示 `Failed Graphics Hardware Qualifier check`，随后退回 `SWPipeline`。这个参数跳过白名单检查。

直接运行 jar 时：

```sh
java -Dprism.forceGPU=true -jar HMCL.jar
```

使用 HMCL 的 `.sh` 启动包时：

```sh
HMCL_JAVA_OPTS="-Dprism.forceGPU=true" bash HMCL.sh
```

Java 需要带适用于 ARM64 的 JavaFX。不要在系统里全局设置 `JAVA_TOOL_OPTIONS`，也不用把显卡名称改成别的厂商。这个参数不会强制 Minecraft 选用某个驱动，游戏仍按自己的 LWJGL / OpenGL 流程初始化。

## 怎样确认生效

临时加上 `-Dprism.verbose=true`，日志里会显示用的是哪条渲染管线。在 HMCL 3.17.0.359 和 Zulu Java 25.0.4.1 上，加参数前是 `SWPipeline`，加参数后是 `ES2Pipeline`，`glxinfo -B` 同时显示 Adreno 830v1 和 `Accelerated: yes`。这说明启动器走了 GPU 渲染，不代表游戏帧率。

JavaFX 里对应的实现是 [X11GLFactory.java](https://github.com/openjdk/jfx/blob/master/modules/javafx.graphics/src/main/java/com/sun/prism/es2/X11GLFactory.java)。
