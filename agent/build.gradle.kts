plugins {
    java
}

group = "com.myiui"
version = "1.0.0"

java {
    toolchain {
        languageVersion.set(JavaLanguageVersion.of(21))
    }
}

repositories {
    mavenCentral()
}

dependencies {
    compileOnly("org.ow2.asm:asm:9.7.1")
    compileOnly("org.ow2.asm:asm-commons:9.7.1")
    implementation("org.bytedeco:javacv-platform:1.5.11")
    implementation("com.google.code.gson:gson:2.11.0")
    implementation("com.google.zxing:core:3.5.3")
    implementation("com.google.zxing:javase:3.5.3")
}

tasks.jar {
    manifest {
        attributes(
            "Main-Class" to "com.myiui.agent.Main",
            "Implementation-Title" to "MyiUI Agent",
            "Implementation-Version" to project.version
        )
    }
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE
    from(configurations.runtimeClasspath.get().map { if (it.isDirectory) it else zipTree(it) })
}

tasks.register<Exec>("generatePreloaderHeader") {
    dependsOn(tasks.classes)
    val outFile = rootProject.file("../overlay/src/preload/preloader_class.h")
    val classFile = layout.buildDirectory.file("classes/java/main/com/myiui/preload/AgentBootstrap.class")
    commandLine(
        "powershell",
        "-ExecutionPolicy", "Bypass",
        "-File", rootProject.file("../tools/generate_preloader_class.ps1"),
        "-ClassFile", classFile.get().asFile.absolutePath,
        "-OutputFile", outFile.absolutePath
    )
}

tasks.named("jar") {
    dependsOn("generatePreloaderHeader")
}
