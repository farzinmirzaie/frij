# `pio run` only BUILDS. The native emulator has no chip to flash, so this hook
# repurposes the "upload" action (and adds an "Execute" button in the IDE) to
# RUN the built binary — i.e. open the SDL window.
Import("env")

from SCons.Script import AlwaysBuild

program = "${BUILD_DIR}/${PROGNAME}${PROGSUFFIX}"

# `pio run -t upload` -> run the program
AlwaysBuild(env.Alias("upload", program, program))

# "Execute" target / button in the PlatformIO IDE explorer
env.AddTarget(
    name="execute",
    dependencies=program,
    actions=program,
    title="Execute",
    description="Build and run",
    group="General",
)
