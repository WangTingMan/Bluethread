# 贡献指南

欢迎参与 Bluethread 项目贡献！无论提交 Bug 反馈、文档修正、代码功能实现，都非常感谢你的参与。

## 提交代码基本流程（推荐 Fork + PR 模式）

本仓库采用 **Pull Request（PR）** 作为代码合并入口，不允许直接向 `main` 分支推送代码。

### 1. Fork 仓库

访问仓库主页，点击右上角 `Fork`，将仓库复制到你自己的 GitHub 账号下。

### 2. 克隆代码到本地

```
# 克隆你 Fork 之后的仓库
git clone git@github.com:你的用户名/Bluethread.git
cd Bluethread

# 添加上游仓库（用于同步主仓库最新代码）
git remote add upstream https://github.com/WangTingMan/Bluethread.git
```

### 3. 创建开发分支

**不要在 main 分支上直接修改代码**，新建独立分支开发：

```
# 切换并新建分支
# 分支命名规范：feature/xxx 新增功能；fix/xxx 修复bug；docs/xxx文档修改
git checkout -b feature/rfcomm-credit
```

### 4. 编码、自测与提交

1. 修改代码；
2. 在本地完成编译、单元测试、功能验证，保证代码可正常编译运行；
3. 遵循项目代码风格；
4. 提交代码：

```
git add .
git commit -m "rfcomm: 增加credit流控支持"
```

> 
> commit 描述建议简明，优先英文，方便 git log 检索。

### 5. 推送分支到你自己的 Fork 仓库

```
git push origin feature/rfcomm-credit
```

### 6. 创建 Pull Request（PR）

1. 打开你的 Fork 仓库页面，会看到提示：`Compare & pull request`，点击进入创建 PR 页面；
2. 确认仓库与分支选择：
   - 基础仓库 (base repository)：`WangTingMan/Bluethread`
   - 基础分支 (base branch)：`main`
   - 头仓库 (head repository)：你自己 Fork 的仓库
   - 对比分支 (compare branch)：你刚才推送的功能分支
3. 填写 PR 标题和描述：
   - 标题：简短概括改动内容；
   - 描述：写明改动目的、实现内容、测试环境与测试结果；
   - 如果对应已有 Issue，可以写 `Closes #编号`，合并 PR 后自动关闭该 Issue。
4. 如果代码尚未完成，可选择创建**草稿 PR（Draft PR）**，标记为 WIP，等待完成后再取消草稿状态等待评审。
5. 点击创建 PR。

## 7. 代码评审与修改

- 维护者会对 PR 进行代码审查，在代码行或页面留下评论；
- 根据评审意见，在本地分支继续修改代码，再次 commit、push；
- push 到同一分支后，PR 会**自动更新**，不需要新建 PR；
- 尽量避免对正在评审的分支执行 `git push --force`，会破坏评审记录。

## 8. 合并 PR

维护者评审通过后，会选择合并。推荐使用 **Squash and merge**，将本次 PR 所有提交压缩为一条提交记录，保持主仓库提交历史整洁。
合并完成后，临时开发分支可以删除。

## 同步上游仓库（主仓库）最新代码

当主仓库 `main` 分支有更新，你的本地分支落后时，执行：

```
# 拉取主仓库最新代码
git fetch upstream
# 合并上游main到本地分支
git merge upstream/main
# 如有冲突，手动解决冲突后再提交推送
```

## 分支命名规范

- `feature/xxx`：新增功能
- `fix/xxx`：bug 修复
- `docs/xxx`：文档、注释修改
- `refactor/xxx`：代码重构，不改变行为
- `ci/xxx`：CI 编译脚本修改

## 代码规范

1. 遵循项目现有代码风格；
2. 新增代码补充必要注释，协议相关逻辑尽量写清楚；
3. 提交前本地编译验证，尽量不要提交编译错误代码；
4. 尽量小批量提交，一个 PR 只解决一类问题，不要混合大量无关改动。

## 提交 Issue

发现 Bug 或者需求建议，可以直接新建 Issue：

- Bug 反馈：写明复现步骤、平台、现象、预期行为；
- 新功能建议：描述使用场景与需求。

## 许可声明

提交代码即代表你同意将你的贡献以本仓库协议（GPLv3）开源。