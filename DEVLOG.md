# DEVLOG and LESSONS

## LESSONS

- When creating a new Parameter (for apvts), do not use "String-Like", explicitly call ParameterID constructor with versionHint. 

## DEVLOG

### 2025-01-07  – JUCE Parameters
**Problem:** Plugin crashed on load  
**Cause:** juce::ParameterID had no versionHint
**Fix:** Always set versionHint when adding new params

---
