#include "command.hh"
#include "store-api.hh"
#include "derivations.hh"
#include "profiles.hh"

namespace nix {

Commands * RegisterCommand::commands = nullptr;

StoreCommand::StoreCommand()
{
}

ref<Store> StoreCommand::getStore()
{
    if (!_store)
        _store = createStore();
    return ref<Store>(_store);
}

ref<Store> StoreCommand::createStore()
{
    return openStore();
}

void StoreCommand::run()
{
    run(getStore());
}

StorePathsCommand::StorePathsCommand(bool recursive)
    : recursive(recursive)
{
    if (recursive)
        mkFlag()
            .longName("no-recursive")
            .description("apply operation to specified paths only")
            .set(&this->recursive, false);
    else
        mkFlag()
            .longName("recursive")
            .shortName('r')
            .description("apply operation to closure of the specified paths")
            .set(&this->recursive, true);

    mkFlag(0, "all", "apply operation to the entire store", &all);
}

void StorePathsCommand::run(ref<Store> store)
{
    Paths storePaths;

    if (all) {
        if (installables.size())
            throw UsageError("'--all' does not expect arguments");
        for (auto & p : store->queryAllValidPaths())
            storePaths.push_back(p);
    }

    else {
        for (auto & p : toStorePaths(store, realiseMode, installables))
            storePaths.push_back(p);

        if (recursive) {
            PathSet closure;
            store->computeFSClosure(PathSet(storePaths.begin(), storePaths.end()),
                closure, false, false);
            storePaths = Paths(closure.begin(), closure.end());
        }
    }

    run(store, storePaths);
}

void StorePathCommand::run(ref<Store> store)
{
    auto storePaths = toStorePaths(store, NoBuild, installables);

    if (storePaths.size() != 1)
        throw UsageError("this command requires exactly one store path");

    run(store, *storePaths.begin());
}

MixProfile::MixProfile()
{
    mkFlag()
        .longName("profile")
        .description("profile to update")
        .labels({"path"})
        .dest(&profile);
}

void MixProfile::updateProfile(const Path & storePath)
{
    if (!profile) return;
    auto store = getStore().dynamic_pointer_cast<LocalFSStore>();
    if (!store) throw Error("'--profile' is not supported for this Nix store");
    auto profile2 = absPath(*profile);
    switchLink(profile2,
        createGeneration(
            ref<LocalFSStore>(store),
            profile2, storePath));
}

void MixProfile::updateProfile(const Buildables & buildables)
{
    if (!profile) return;

    std::optional<Path> result;

    for (auto & buildable : buildables) {
        for (auto & output : buildable.outputs) {
            if (result)
                throw Error("'--profile' requires that the arguments produce a single store path, but there are multiple");
            result = output.second;
        }
    }

    if (!result)
        throw Error("'--profile' requires that the arguments produce a single store path, but there are none");

    updateProfile(*result);
}

MixDefaultProfile::MixDefaultProfile()
{
    profile = getDefaultProfile();
}

MixEnvironment::MixEnvironment() : ignoreEnvironment(false) {
    mkFlag()
        .longName("ignore-environment")
        .shortName('i')
        .description("clear the entire environment (except those specified with --keep)")
        .set(&ignoreEnvironment, true);

    mkFlag()
        .longName("keep")
        .shortName('k')
        .description("keep specified environment variable")
        .arity(1)
        .labels({"name"})
        .handler([&](std::vector<std::string> ss) { keep.insert(ss.front()); });

    mkFlag()
        .longName("unset")
        .shortName('u')
        .description("unset specified environment variable")
        .arity(1)
        .labels({"name"})
        .handler([&](std::vector<std::string> ss) { unset.insert(ss.front()); });
}

void MixEnvironment::setEnviron() {
    if (ignoreEnvironment) {
        if (!unset.empty())
            throw UsageError("--unset does not make sense with --ignore-environment");

        for (const auto & var : keep) {
            auto val = getenv(var.c_str());
            if (val) stringEnv.emplace_back(fmt("%s=%s", var.c_str(), val));
        }
        vectorEnv = stringsToCharPtrs(stringsEnv);
        environ = vectorEnv.data();
    } else {
        if (!keep.empty())
            throw UsageError("--keep does not make sense without --ignore-environment");

        for (const auto & var : unset)
            unsetenv(var.c_str());
    }
}

}
