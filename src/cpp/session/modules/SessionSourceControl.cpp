/*
 * SessionSourceControl.cpp
 *
 * Copyright (C) 2009-11 by RStudio, Inc.
 *
 * This program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
#include "SessionSourceControl.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <core/json/JsonRpc.hpp>
#include <core/system/System.hpp>
#include <core/Exec.hpp>
#include <core/FileSerializer.hpp>
#include <core/StringUtils.hpp>

#include "session/SessionModuleContext.hpp"

namespace session {
namespace modules {
namespace source_control {

namespace {

enum PatchMode
{
   PatchModeWorking = 0,
   PatchModeStage = 1
};

class VCSImpl : boost::noncopyable
{
public:
   VCSImpl()
   {
      root_ = module_context::activeProjectDirectory();
   }

   virtual ~VCSImpl()
   {
   }

   virtual VCS id() { return VCSNone; }
   virtual std::string name() { return std::string(); }

   virtual core::Error status(const FilePath&,
                              StatusResult* pStatusResult)
   {
      *pStatusResult = StatusResult();
      return Success();
   }

   virtual core::Error add(const std::vector<FilePath>& filePaths,
                           std::string* pStdErr)
   {
      return Success();
   }

   virtual core::Error remove(const std::vector<FilePath>& filePaths,
                              std::string* pStdErr)
   {
      return Success();
   }

   virtual core::Error revert(const std::vector<FilePath>& filePaths,
                              std::string* pStdErr)
   {
      return Success();
   }

   virtual core::Error unstage(const std::vector<FilePath>& filePaths,
                               std::string* pStdErr)
   {
      return Success();
   }

   core::Error runCommand(const std::string& command,
                          const std::vector<std::string>& args,
                          std::vector<std::string>* pOutputLines=NULL)
   {
      std::string output;
      Error error = runCommand(command,
                               args,
                               &output);
      if (error)
         return error;

      if (pOutputLines)
      {
         boost::algorithm::split(*pOutputLines, output,
                                 boost::algorithm::is_any_of("\r\n"));
      }

      return Success();
   }

   core::Error runCommand(const std::string& command,
                          const std::vector<std::string>& args,
                          std::string* pOutput)
   {
      std::string cmd("cd ");
      cmd.append(string_utils::bash_escape(root_));
      cmd.append("; ");
      cmd.append(command);
      for (std::vector<std::string>::const_iterator it = args.begin();
           it != args.end();
           it++)
      {
         cmd.append(" ");
         cmd.append(string_utils::bash_escape(*it));
      }

      Error error = core::system::captureCommand(cmd, pOutput);
      if (error)
         return error;

      return Success();
   }

   virtual core::Error diffFile(FilePath filePath,
                                PatchMode mode,
                                int contextLines,
                                std::string* pOutput)
   {
      return Success();
   }

   virtual core::Error applyPatch(FilePath patchFile,
                                  PatchMode patchMode,
                                  std::string* pStdErr)
   {
      return Success();
   }

protected:
   FilePath root_;
};

boost::scoped_ptr<VCSImpl> s_pVcsImpl_;

class GitVCSImpl : public VCSImpl
{
public:
   VCS id() { return VCSGit; }
   std::string name() { return "Git"; }

   core::Error status(const FilePath& dir, StatusResult* pStatusResult)
   {
      using namespace boost;

      std::vector<FileWithStatus> files;

      std::vector<std::string> args;
      args.push_back("status");
      args.push_back("--porcelain");
      args.push_back("--");
      args.push_back(string_utils::utf8ToSystem(dir.absolutePath()));

      std::vector<std::string> lines;
      Error error = runCommand("git", args, &lines);
      if (error)
         return error;

      for (std::vector<std::string>::iterator it = lines.begin();
           it != lines.end();
           it++)
      {
         std::string line = *it;
         if (line.length() < 4)
            continue;
         FileWithStatus file;

         file.status = line.substr(0, 2);

         std::string filePath = line.substr(3);
         if (filePath.length() > 1 && filePath[filePath.length() - 1] == '/')
            filePath = filePath.substr(0, filePath.size() - 1);
         file.path = root_.childPath(string_utils::systemToUtf8(filePath));

         files.push_back(file);
      }

      *pStatusResult = StatusResult(files);

      return Success();
   }

   core::Error doSimpleCmd(const std::string& command,
                           const std::vector<std::string>& options,
                           const std::vector<FilePath>& filePaths,
                           std::string* pStdErr)
   {
      std::vector<std::string> args;
      args.push_back(command);
      for (std::vector<std::string>::const_iterator it = options.begin();
           it != options.end();
           it++)
      {
         args.push_back(*it);
      }
      args.push_back("--");
      for (std::vector<FilePath>::const_iterator it = filePaths.begin();
           it != filePaths.end();
           it++)
      {
        args.push_back(string_utils::utf8ToSystem((*it).absolutePath()));
      }

      runCommand("git", args);

      // TODO: Once we capture stderr we need to set it here
      if (pStdErr)
         *pStdErr = std::string();

      return Success();
   }

   core::Error add(const std::vector<FilePath>& filePaths,
                   std::string* pStdErr)
   {
      return doSimpleCmd("add",
                         std::vector<std::string>(),
                         filePaths,
                         pStdErr);
   }

   core::Error remove(const std::vector<FilePath>& filePaths,
                      std::string* pStdErr)
   {
      return doSimpleCmd("rm",
                         std::vector<std::string>(),
                         filePaths,
                         pStdErr);
   }

   core::Error revert(const std::vector<FilePath>& filePaths,
                      std::string* pStdErr)
   {
      std::vector<std::string> args;
      args.push_back("HEAD");
      Error error = doSimpleCmd("reset",  args, filePaths, pStdErr);
      if (error)
         return error;

      return doSimpleCmd("checkout",
                         std::vector<std::string>(),
                         filePaths,
                         pStdErr);
   }

   core::Error unstage(const std::vector<FilePath>& filePaths,
                       std::string* pStdErr)
   {
      std::vector<std::string> args;
      args.push_back("HEAD");
      return doSimpleCmd("reset", args, filePaths, pStdErr);
   }

   core::Error commit(const std::string& message, bool amend, bool signOff)
   {
      FilePath tempFile = module_context::tempFile("gitmsg", "txt");
      boost::shared_ptr<std::ostream> pStream;

      Error error = tempFile.open_w(&pStream);
      if (error)
         return error;

      *pStream << message;
      pStream->flush();

      std::vector<std::string> args;
      args.push_back("-F");
      args.push_back(string_utils::utf8ToSystem(tempFile.absolutePath()));
      if (amend)
         args.push_back("--amend");
      if (signOff)
         args.push_back("--signoff");

      std::string stdErr;
      error = doSimpleCmd("commit", args, std::vector<FilePath>(), &stdErr);

      // clean up commit message temp file
      Error removeError = tempFile.remove();
      if (removeError)
         LOG_ERROR(removeError);

      return error;
   }

   virtual core::Error diffFile(FilePath filePath,
                                PatchMode mode,
                                int contextLines,
                                std::string* pOutput)
   {
      std::vector<std::string> args;
      args.push_back("diff");
      args.push_back("-U" + boost::lexical_cast<std::string>(contextLines));
      if (mode == PatchModeStage)
         args.push_back("--cached");
      args.push_back("--");
      args.push_back(string_utils::bash_escape(filePath));

      runCommand("git", args, pOutput);

      return Success();
   }

   virtual core::Error applyPatch(FilePath patchFile,
                                  PatchMode patchMode,
                                  std::string* pStdErr)
   {
      std::vector<std::string> args;

      if (patchMode == PatchModeStage)
         args.push_back("--cached");

      std::vector<FilePath> filePaths;
      filePaths.push_back(patchFile);

      return doSimpleCmd("apply", args, filePaths, pStdErr);
   }
};

class SubversionVCSImpl : public VCSImpl
{
   VCS id() { return VCSSubversion; }
   std::string name() { return "SVN"; }

   core::Error status(const FilePath& dir, StatusResult* pStatusResult)
   {
      using namespace boost;

      std::vector<FileWithStatus> files;

      std::vector<std::string> args;
      args.push_back("status");
      args.push_back("--non-interactive");
      args.push_back("--depth=immediates");
      args.push_back("--");
      args.push_back(string_utils::utf8ToSystem(dir.absolutePath()));

      std::vector<std::string> lines;
      Error error = runCommand("svn", args, &lines);
      if (error)
         return error;

      for (std::vector<std::string>::iterator it = lines.begin();
           it != lines.end();
           it++)
      {
         std::string line = *it;
         if (line.length() < 4)
            continue;
         FileWithStatus file;

         file.status = line.substr(0, 7);

         std::string filePath = line.substr(8);
         if (filePath.length() > 1 && filePath[filePath.length() - 1] == '/')
            filePath = filePath.substr(0, filePath.size() - 1);
         file.path = FilePath(string_utils::systemToUtf8(filePath));
         files.push_back(file);
      }

      *pStatusResult = StatusResult(files);

      return Success();
   }

   core::Error revert(const std::vector<FilePath>& filePaths,
                      std::string* pStdErr)
   {
      std::vector<std::string> args;
      args.push_back("revert");
      args.push_back("--");
      for (std::vector<FilePath>::const_iterator it = filePaths.begin();
           it != filePaths.end();
           it++)
      {
        args.push_back(string_utils::utf8ToSystem((*it).absolutePath()));
      }

      runCommand("svn", args);

      // TODO: Once we capture stderr we need to set it here
      if (pStdErr)
         *pStdErr = std::string();

      return Success();
   }
};

std::vector<FilePath> resolveAliasedPaths(json::Array paths)
{
   std::vector<FilePath> parsedPaths;
   for (json::Array::iterator it = paths.begin();
        it != paths.end();
        it++)
   {
      json::Value value = *it;
      parsedPaths.push_back(
            module_context::resolveAliasedPath(value.get_str()));
   }
   return parsedPaths;
}

} // anonymous namespace


VCS activeVCS()
{
   return s_pVcsImpl_->id();
}

std::string activeVCSName()
{
   return s_pVcsImpl_->name();
}

VCSStatus StatusResult::getStatus(const FilePath& fileOrDirectory) const
{
   std::map<std::string, VCSStatus>::const_iterator found =
         this->filesByPath_.find(fileOrDirectory.absolutePath());
   if (found != this->filesByPath_.end())
      return found->second;

   return VCSStatus();
}

core::Error status(const FilePath& dir, StatusResult* pStatusResult)
{
   return s_pVcsImpl_->status(dir, pStatusResult);
}

Error vcsAdd(const json::JsonRpcRequest& request,
             json::JsonRpcResponse* pResponse)
{
   json::Array paths;
   Error error = json::readParam(request.params, 0, &paths);
   if (error)
      return error ;

   return s_pVcsImpl_->add(resolveAliasedPaths(paths), NULL);
}

Error vcsRemove(const json::JsonRpcRequest& request,
                json::JsonRpcResponse* pResponse)
{
   json::Array paths;
   Error error = json::readParam(request.params, 0, &paths);
   if (error)
      return error ;

   return s_pVcsImpl_->remove(resolveAliasedPaths(paths), NULL);
}

Error vcsRevert(const json::JsonRpcRequest& request,
                json::JsonRpcResponse* pResponse)
{
   json::Array paths;
   Error error = json::readParam(request.params, 0, &paths);
   if (error)
      return error ;

   return s_pVcsImpl_->revert(resolveAliasedPaths(paths), NULL);
}

Error vcsUnstage(const json::JsonRpcRequest& request,
                 json::JsonRpcResponse* pResponse)
{
   json::Array paths;
   Error error = json::readParam(request.params, 0, &paths);
   if (error)
      return error ;

   return s_pVcsImpl_->unstage(resolveAliasedPaths(paths), NULL);
}

Error vcsFullStatus(const json::JsonRpcRequest&,
                    json::JsonRpcResponse* pResponse)
{
   StatusResult statusResult;
   Error error = s_pVcsImpl_->status(module_context::activeProjectDirectory(),
                                     &statusResult);
   if (error)
      return error;

   std::vector<FileWithStatus> files = statusResult.files();
   json::Array result;
   for (std::vector<FileWithStatus>::const_iterator it = files.begin();
        it != files.end();
        it++)
   {
      VCSStatus status = it->status;
      FilePath path = it->path;
      json::Object obj;
      obj["status"] = status.status();
      obj["path"] = path.relativePath(module_context::activeProjectDirectory());
      obj["raw_path"] = path.absolutePath();
      result.push_back(obj);
   }

   pResponse->setResult(result);

   return Success();
}

Error vcsCommitGit(const json::JsonRpcRequest& request,
                   json::JsonRpcResponse* pResponse)
{
   GitVCSImpl* pGit = dynamic_cast<GitVCSImpl*>(s_pVcsImpl_.get());
   if (!pGit)
      return systemError(boost::system::errc::operation_not_supported, ERROR_LOCATION);

   std::string commitMsg;
   bool amend, signOff;
   Error error = json::readParams(request.params, &commitMsg, &amend, &signOff);
   if (error)
      return error;

   error = pGit->commit(commitMsg, amend, signOff);
   if (error)
      return error;

   return Success();
}

Error vcsDiffFile(const json::JsonRpcRequest& request,
                  json::JsonRpcResponse* pResponse)
{
   std::string path;
   int mode;
   Error error = json::readParams(request.params, &path, &mode);
   if (error)
      return error;

   std::string output;
   error = s_pVcsImpl_->diffFile(module_context::resolveAliasedPath(path),
                                 static_cast<PatchMode>(mode),
                                 999999999,
                                 &output);
   if (error)
      return error;

   pResponse->setResult(output);
   return Success();
}

Error vcsApplyPatch(const json::JsonRpcRequest& request,
                    json::JsonRpcResponse* pResponse)
{
   std::string patch;
   int mode;
   Error error = json::readParams(request.params, &patch, &mode);
   if (error)
      return error;

   FilePath patchFile = module_context::tempFile("rstudiovcs", "patch");
   error = writeStringToFile(patchFile, patch);
   if (error)
      return error;

   error = s_pVcsImpl_->applyPatch(patchFile, static_cast<PatchMode>(mode), NULL);

   Error error2 = patchFile.remove();
   if (error2)
      LOG_ERROR(error2);

   if (error)
      return error;

   return Success();
}

core::Error initialize()
{
   FilePath workingDir = module_context::activeProjectDirectory();
   if (workingDir.empty())
      s_pVcsImpl_.reset(new VCSImpl());
   else if (workingDir.childPath(".git").isDirectory())
      s_pVcsImpl_.reset(new GitVCSImpl());
   else if (workingDir.childPath(".svn").isDirectory())
      s_pVcsImpl_.reset(new SubversionVCSImpl());
   else
      s_pVcsImpl_.reset(new VCSImpl());

   // install rpc methods
   using boost::bind;
   using namespace module_context;
   ExecBlock initBlock ;
   initBlock.addFunctions()
      (bind(registerRpcMethod, "vcs_add", vcsAdd))
      (bind(registerRpcMethod, "vcs_remove", vcsRemove))
      (bind(registerRpcMethod, "vcs_revert", vcsRevert))
      (bind(registerRpcMethod, "vcs_unstage", vcsUnstage))
      (bind(registerRpcMethod, "vcs_full_status", vcsFullStatus))
      (bind(registerRpcMethod, "vcs_commit_git", vcsCommitGit))
      (bind(registerRpcMethod, "vcs_diff_file", vcsDiffFile))
      (bind(registerRpcMethod, "vcs_apply_patch", vcsApplyPatch));
   Error error = initBlock.execute();
   if (error)
      return error;

   return Success();
}

} // namespace source_control
} // namespace modules
} // namespace session
