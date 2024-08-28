function main()
    local platform = os.host()
    local arch = os.arch()
    local dir = ".xmake\\" .. platform .. "\\" .. arch .. "\\repositories\\liteldev-repo\\packages\\s\\sqlitecpp"
    if os.isdir(dir) then
        os.rm(dir)
    end
end