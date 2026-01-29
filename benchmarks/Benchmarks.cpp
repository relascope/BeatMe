TEST_CASE ("Boot performance")
{
    BENCHMARK ("Processor constructor")
    {
        return PluginProcessor();
    };

    BENCHMARK ("Processor destructor")
    {
        auto p = std::make_unique<PluginProcessor>();
        p.reset();
        return p;
    };

    BENCHMARK ("Editor open and close")
    {
        PluginProcessor plugin;
        auto editor = plugin.createEditorIfNeeded();
        plugin.editorBeingDeleted (editor);
        delete editor;
        return plugin.getActiveEditor();
    };
}
