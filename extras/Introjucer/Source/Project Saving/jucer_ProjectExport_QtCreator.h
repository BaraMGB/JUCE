﻿/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2013 - Raw Material Software Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

class QtCreatorProjectExporter  : public ProjectExporter
{
public:
    //==============================================================================
    static const char* getName()                { return "QtCreator";  }
    static const char* getValueTreeTypeName()   { return "QT_CREATOR"; }

    static QtCreatorProjectExporter* createForSettings (Project& project, const ValueTree& settings)
    {
        if (settings.hasType (getValueTreeTypeName()))
            return new QtCreatorProjectExporter (project, settings);

        return nullptr;
    }


    //==============================================================================
    QtCreatorProjectExporter (Project& p, const ValueTree& t)   : ProjectExporter (p, t)
    {
        name = getName();

        if (getTargetLocationString().isEmpty())
            getTargetLocationValue() = getDefaultBuildsRootFolder() + "QtCreator";
    }

    //==============================================================================
    bool canLaunchProject() override                    { return false; }
    bool launchProject() override                       { return false; }
    bool usesMMFiles() const override                   { return false; }
    bool isQtCreator() const override                   { return true; }
    bool canCopeWithDuplicateFiles() override           { return false; }

    void createExporterProperties (PropertyListBuilder&) override
    {
    }

    //==============================================================================
    void create (const OwnedArray<LibraryModule>&) const override
    {
        Array<RelativePath> sourceFiles;
        Array<RelativePath> headerFiles;
        for (int i = 0; i < getAllGroups().size(); ++i) {
            findAllSourceFiles (getAllGroups().getReference(i), sourceFiles);
            findAllHeaderFiles (getAllGroups().getReference(i), headerFiles);
        }
        MemoryOutputStream mo;
        writeMakefile (mo, sourceFiles, headerFiles);

        overwriteFileIfDifferentOrThrow (getTargetFolder()
                                         .getChildFile (project.getProjectFilenameRoot())
                                         .withFileExtension (".pro"), mo);
    }

protected:
    //==============================================================================
    class QtCreatorBuildConfiguration  : public BuildConfiguration
    {
    public:
        QtCreatorBuildConfiguration (Project& p, const ValueTree& settings)
            : BuildConfiguration (p, settings)
        {
            setValueIfVoid (getLibrarySearchPathValue(), "/usr/X11R6/lib/");
        }

        Value getArchitectureType()                 { return getValue (Ids::linuxArchitecture); }
        String getArchitectureTypeString() const    { return config [Ids::linuxArchitecture]; }

        void createConfigProperties (PropertyListBuilder& props) override
        {
            /*static const char* const archNames[] = { "(Default)", "32-bit (-m32)", "64-bit (-m64)", "ARM v6", "ARM v7" };
            const var archFlags[] = { var(), "-m32", "-m64", "-march=armv6", "-march=armv7" };

            props.add (new ChoicePropertyComponent (getArchitectureType(), "Architecture",
                                                    StringArray (archNames, numElementsInArray (archNames)),
                                                    Array<var> (archFlags, numElementsInArray (archFlags)))); */
        }
    };

    BuildConfiguration::Ptr createBuildConfig (const ValueTree& tree) const override
    {
        return new QtCreatorBuildConfiguration (project, tree);
    }

private:
    //==============================================================================
    void findAllSourceFiles (const Project::Item& projectItem, Array<RelativePath>& results) const
    {
        if (projectItem.isGroup())
        {
            String sName = projectItem.getName();
            int inumChild = projectItem.getNumChildren();
            for (int i = 0; i < projectItem.getNumChildren(); ++i)
                findAllSourceFiles(projectItem.getChild(i), results);
        }
        else
        {
            String sName = projectItem.getName();
            if (projectItem.shouldBeCompiled())
                results.add (RelativePath (projectItem.getFile(), getTargetFolder(), RelativePath::buildTargetFolder));
        }
    }

    void findAllHeaderFiles (const Project::Item& projectItem, Array<RelativePath>& results) const
    {
        if (projectItem.isGroup())
        {
            for (int i = 0; i < projectItem.getNumChildren(); ++i)
                findAllHeaderFiles(projectItem.getChild(i), results);
        }
        else
        {
            if (projectItem.getFile().hasFileExtension (headerFileExtensions))
                results.add (RelativePath (projectItem.getFile(), getTargetFolder(), RelativePath::buildTargetFolder));
        }
    }

    void writeMakefile (OutputStream& out, const Array<RelativePath>& sourceFiles, const Array<RelativePath>& headerFiles) const
    {
        out << "# Automatically generated qmake file, created by the Introjucer" << newLine
            << "# Don't edit this file! Your changes will be overwritten when you re-save the Introjucer project!" << newLine
            << newLine;

        out << "TEMPLATE = app" << newLine;
        out << "CONFIG  -= qt" << newLine;
        out << newLine;
        out << "CONFIG(release, debug|release){" << newLine
            << "    DESTDIR     = build/release/" << newLine
            << "    OBJECTS_DIR = build/release/intermediate/" << newLine;
        for (ConstConfigIterator config (*this); config.next();)
            if (!config->isDebug())
                out << "    TARGET = " << config->getTargetBinaryNameString() << newLine;
        out << "}" << newLine;

        out << "CONFIG(debug, debug|release){" << newLine
            << "    DESTDIR     = build/debug/" << newLine
            << "    OBJECTS_DIR = build/debug/intermediate/" << newLine;
        for (ConstConfigIterator config (*this); config.next();)
            if (config->isDebug())
               out << "    TARGET = " << config->getTargetBinaryNameString() << newLine;
        out << "}" << newLine;
        out << newLine;

        out << "# Compiler flags" << newLine;

        // general options
        out << "QMAKE_CFLAGS = -std=gnu++0x";
        if (makefileIsDLL)
            out << " -fPIC";
        out << newLine;

        // defines
        //out << createGCCPreprocessorFlags (getAllPreprocessorDefs (config));

        StringPairArray defines;
        for (ConstConfigIterator config (*this); config.next();) {
            if (!config->isDebug()) {
                // Release specific defines
                defines.clear();
                defines.set ("NDEBUG", "1");
                out << "QMAKE_CFLAGS_RELEASE = ";
                out << " -O" << config->getGCCOptimisationFlag();
                out << (" "  + replacePreprocessorTokens (*config, getExtraCompilerFlagsString())).trimEnd();
                out << createGCCPreprocessorFlags (mergePreprocessorDefs (defines, getAllPreprocessorDefs (*config)));
                out << newLine;

                // include paths
                StringArray searchPaths (extraSearchPaths);
                searchPaths.addArray (config->getHeaderSearchPaths());
                searchPaths.removeDuplicates (false);
                out << "CONFIG(release, debug|release){" << newLine
                    << "    INCLUDEPATH = \\" << newLine;
                for (int i = 0; i < searchPaths.size(); ++i)
                    out << "        " << addQuotesIfContainsSpaces (FileHelpers::unixStylePath (replacePreprocessorTokens (*config, searchPaths[i]))) << " \\" << newLine;
                out << "}" << newLine;

            } else {
                // Debug specific defines
                defines.clear();
                defines.set ("DEBUG", "1");
                defines.set ("_DEBUG", "1");
                out << "QMAKE_CFLAGS_DEBUG   = -g -ggdb ";
                out << " -O" << config->getGCCOptimisationFlag();
                out << (" "  + replacePreprocessorTokens (*config, getExtraCompilerFlagsString())).trimEnd();
                out << createGCCPreprocessorFlags (mergePreprocessorDefs (defines, getAllPreprocessorDefs (*config)));
                out << newLine;

                // include paths
                StringArray searchPaths (extraSearchPaths);
                searchPaths.addArray (config->getHeaderSearchPaths());
                searchPaths.removeDuplicates (false);
                out << "CONFIG(debug, debug|release){" << newLine
                    << "    INCLUDEPATH = \\" << newLine;
                for (int i = 0; i < searchPaths.size(); ++i)
                    out << "        " << addQuotesIfContainsSpaces (FileHelpers::unixStylePath (replacePreprocessorTokens (*config, searchPaths[i]))) << " \\" << newLine;
                out << "}" << newLine;
            }
        }


        // Linux specific options
        defines.clear();
        out << "unix:  QMAKE_CFLAGS += -I/usr/include/freetype2 -I/usr/include";
        defines.set ("LINUX", "1");
        out << createGCCPreprocessorFlags (defines);
        out << newLine;

        // Windows specific options
        defines.clear();
        out << "win32: QMAKE_CFLAGS += -mstackrealign -D__MINGW__=1 -D__MINGW_EXTENSION=";
        out << createGCCPreprocessorFlags (defines);
        out << newLine;

        out << newLine  << newLine;

        // Copy flags from C to CXX
        out << "QMAKE_CXXFLAGS         = $$QMAKE_CFLAGS"         << newLine
            << "QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE" << newLine
            << "QMAKE_CXXFLAGS_DEBUG   = $$QMAKE_CFLAGS_DEBUG"   << newLine;

        out << newLine  << newLine;

        // Linker flags
        out << "# Linker flags" << newLine;
        out << "LIBS = -L$$DESTDIR " << getExternalLibrariesString();
        if (makefileIsDLL)
            out << " -shared";
         out << newLine;

        // Linux specific linker flags
        out << "unix:  LIBS += -L/usr/X11R6/lib/";
        for (int i = 0; i < linuxLibs.size(); ++i)
            out << " -l" << linuxLibs[i];
        out << newLine;

        // Windows specific linker flags
        out << "win32: LIBS += -lgdi32 -luser32 -lkernel32 -lcomctl32";
        for (int i = 0; i < mingwLibs.size(); ++i)
            out << " -l" << mingwLibs[i];
        out << newLine;

        // Debug specific linker flags
        out << "QMAKE_LFLAGS_DEBUG += -fvisibility=hidden" << newLine;

        // Collect all source files
        out << newLine << "SOURCES = \\" << newLine;
        for (int i = 0; i < sourceFiles.size(); ++i)
        {
            //if (shouldFileBeCompiledByDefault (sourceFiles.getReference(i)))
            //{
                jassert (sourceFiles.getReference(i).getRoot() == RelativePath::buildTargetFolder);
                out << "\t\"" << (sourceFiles.getReference(i).toUnixStyle()) << "\" \\" << newLine;
            //}
        }
        out << newLine;

        // Collect all header files
        out << newLine << "HEADERS = \\" << newLine;
        for (int i = 0; i < headerFiles.size(); ++i)
        {
            jassert (headerFiles.getReference(i).getRoot() == RelativePath::buildTargetFolder);
            out << "\t\"" << (headerFiles.getReference(i).toUnixStyle()) << "\" \\" << newLine;
        }
        out << newLine;

    }

    /*String getArchFlags (const BuildConfiguration& config) const
    {
        if (const MakeBuildConfiguration* makeConfig = dynamic_cast<const MakeBuildConfiguration*> (&config))
            if (makeConfig->getArchitectureTypeString().isNotEmpty())
                return makeConfig->getArchitectureTypeString();

        return "-march=native";
    }*/
    JUCE_DECLARE_NON_COPYABLE (QtCreatorProjectExporter)
};
