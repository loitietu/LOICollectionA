#ifndef LANG_PLUGIN_H
#define LANG_PLUGIN_H

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

static nlohmann::ordered_json CNLangData = {
    {"name", "简体中文"},
    {"exit", "§e[LOICollection] §c已退出"},
    {"exit.gui.title", "§e§l内容提交"},
    {"exit.gui.label", "§a请选择执行内容"},
    {"exit.gui.button1", "§c返回上一级菜单"},
    {"exit.gui.button2", "§b退出菜单"},
    {"language.log", "玩家 {player}({uuid}) 将语言更改为 {language}"},
    {"language.gui.title", "§e§lLOICollection -> §a语言设置"},
    {"language.gui.label", "§a个人语言设置"},
    {"language.gui.lang", "§b当前语言: §r${language}"},
    {"language.gui.dropdown", "§e语言选择"},
    {"blacklist.log1", "已将玩家 {player}({uuid}) 添加进黑名单"},
    {"blacklist.log2", "已将 ${target} 从黑名单中移除"},
    {"blacklist.tips", "§4您已被服务器加入黑名单\n§a请联系管理员解决问题\n§e解除时间: §r${time}\n§b原因: §r${cause}"},
    {"blacklist.gui.title", "§e§lLOICollection -> §b服务器黑名单"},
    {"blacklist.gui.label", "§a功能如下:"},
    {"blacklist.gui.addBlacklist", "§c添加黑名单\n§4将玩家添加至黑名单"},
    {"blacklist.gui.removeBlacklist", "§a删除黑名单\n§4将玩家从黑名单中移除"},
    {"blacklist.gui.add.title", "§eLOIBlacklist §b-> §c添加黑名单"},
    {"blacklist.gui.add.label", "§b请选择添加至黑名单玩家:"},
    {"blacklist.gui.add.dropdown", "请选择添加至黑名单类型:"},
    {"blacklist.gui.add.input1", "请输入加入黑名单原因:"},
    {"blacklist.gui.add.input2", "请输入加入黑名单时间[/hour](0为永久加入)"},
    {"blacklist.gui.remove.title", "§eLOIBlacklist §b-> §a删除黑名单"},
    {"blacklist.gui.remove.label", "§b请选择删除黑名单对象:"},
    {"blacklist.gui.info.label", "§a提交对象: §r${target}\n§d对象名称: §r${player}\n§b提交原因: §r${cause}\n§6提交时间: §r${subtime}\n§e解除时间: §r${time}"},
    {"blacklist.gui.info.remove", "§c删除该黑名单"},
    {"mute.log1", "玩家 {player}({uuid}) 被禁言，原因: ${cause}"},
    {"mute.log2", "目标 ${target} 已解除禁言"},
    {"mute.log3", "玩家 {player}({uuid}) 已被禁言，但仍在尝试发送内容: ${message}"},
    {"mute.tips", "§4您已被禁言，无法发送消息 §e解除时间: §r${time} §b原因: §r${cause}"},
    {"mute.gui.title", "§eLOIMute §b-> §c服务器禁言"},
    {"mute.gui.label", "§a功能如下:"},
    {"mute.gui.addMute", "§c添加禁言\n§4将玩家添加至禁言"},
    {"mute.gui.removeMute", "§a删除禁言\n§4将玩家从禁言中移除"},
    {"mute.gui.add.title", "§eLOIMute §b-> §c添加禁言"},
    {"mute.gui.add.label", "§b请选择添加至禁言玩家:"},
    {"mute.gui.add.input1", "请输入禁言原因:"},
    {"mute.gui.add.input2", "请输入禁言时间[/hour](0为永久禁言)"},
    {"mute.gui.remove.title", "§eLOIMute §b-> §a删除禁言"},
    {"mute.gui.remove.label", "§b请选择删除禁言对象:"},
    {"mute.gui.info.label", "§a提交对象: §r${target}\n§d对象名称: §r${player}\n§b提交原因: §r${cause}\n§6提交时间: §r${subtime}\n§e解除时间: §r${time}"},
    {"mute.gui.info.remove", "§c删除该禁言"},
    {"cdk.log1", "新的CDK: ${cdk} 已创建"},
    {"cdk.log2", "CDK: ${cdk} 已被删除"},
    {"cdk.log3", "玩家 {player}({uuid}) 总换CDK: ${cdk}"},
    {"cdk.tips", "§e当前内容为空，无法继续操作"},
    {"cdk.convert.tips1", "§4CDK不存在，无法总换"},
    {"cdk.convert.tips2", "§4总换失败，该总换码已经使用过了"},
    {"cdk.convert.tips3", "§a总换成功"},
    {"cdk.gui.title", "§eLOICdk §b-> §a总换码"},
    {"cdk.gui.label", "§b功能如下:"},
    {"cdk.gui.convert.input", "§e请输入CDK"},
    {"cdk.gui.addCdk", "§a创建CDK\n§4添加一个空白CDK"},
    {"cdk.gui.removeCdk", "§4删除CDK\n§4删除一个CDK"},
    {"cdk.gui.addAward", "§a添加奖励\n§4将奖励添加至CDK中"},
    {"cdk.gui.new.input1", "§e请输入CDK"},
    {"cdk.gui.new.input2", "§bCDK存在时间[/hour](0为永久存在)"},
    {"cdk.gui.new.switch", "§e是否只能总换一次"},
    {"cdk.gui.remove.dropdown", "§b请选择要删除的CDK"},
    {"cdk.gui.award.score", "§a添加计分板奖励"},
    {"cdk.gui.award.item", "§e添加物品奖励"},
    {"cdk.gui.award.title", "§b添加称号奖励"},
    {"cdk.gui.award.dropdown", "§a请选择要添加的CDK"},
    {"cdk.gui.award.score.input1", "§e请输入计分板名称"},
    {"cdk.gui.award.score.input2", "§b请输入奖励数量"},
    {"cdk.gui.award.item.input1", "§b请输入物品ID"},
    {"cdk.gui.award.item.input2", "§e请输入物品名称"},
    {"cdk.gui.award.item.input3", "§e请输入物品数量"},
    {"cdk.gui.award.item.input4", "§e请输入物品特殊值"},
    {"cdk.gui.award.title.input1", "§e请输入要添加的称号"},
    {"cdk.gui.award.title.input2", "§d请输入称号存在的时间[/hour](0为永久存在)"},
    {"menu.log1", "玩家 {player}({uuid}) 新建菜单 ${menu}"},
    {"menu.log2", "玩家 {player}({uuid}) 删除菜单 ${menu}"},
    {"menu.log3", "玩家 {player}({uuid}) 删除菜单 ${menu} 的组件 ${customize}"},
    {"menu.log4", "玩家 {player}({uuid}) 添加命令至菜单 ${menu}"},
    {"menu.log5", "玩家 {player}({uuid}) 设置菜单 ${menu} 的默认内容"},
    {"menu.log6", "玩家 {player}({uuid}) 向菜单 ${menu} 添加组件"},
    {"menu.gui.title", "§e§lLOICollection -> §b服务器菜单"},
    {"menu.gui.label", "§b功能如下:"},
    {"menu.gui.button1", "§a新建表单\n§4新建一个自定义表单"},
    {"menu.gui.button2", "§b删除表单\n§4删除一个自定义表单"},
    {"menu.gui.button3", "§d添加内容\n§4给自定义表单添加内容"},
    {"menu.gui.button1.input1", "§e请输入表单ID"},
    {"menu.gui.button1.input2", "§e请输入退出表单时返回内容"},
    {"menu.gui.button1.input3", "§e请输入没有指定权限时返回内容"},
    {"menu.gui.button1.input4", "§e请输入没有足够Score时返回内容"},
    {"menu.gui.button1.input5", "§e请输入表单内容"},
    {"menu.gui.button1.input6", "§e请输入表单标题"},
    {"menu.gui.button1.dropdown", "§b请选择表单类型"},
    {"menu.gui.button1.slider", "§b请选择表单权限等级"},
    {"menu.gui.button2.content", "§e§l确定删除表单 §r${menu} §e§l吗"},
    {"menu.gui.button2.yes", "§c确认删除"},
    {"menu.gui.button2.no", "§a取消删除"},
    {"menu.gui.button3.label", "§e§l设置表单 §r${menu}"},
    {"menu.gui.button3.setting", "§d设置内容\n§4设置表单内容"},
    {"menu.gui.button3.new", "§9添加组件\n§4添加自定义组件至表单"},
    {"menu.gui.button3.remove", "§b删除组件\n§4从表单中删除指定组件"},
    {"menu.gui.button3.command", "§a设置命令\n§4设置表单提交时执行命令"},
    {"menu.gui.button3.new.input1", "§e请输入组件ID"},
    {"menu.gui.button3.new.input2", "§e请输入组件标题"},
    {"menu.gui.button3.new.input3", "§e请输入组件图标"},
    {"menu.gui.button3.new.input4", "§e请输入使用该组件时所需要的Score目标"},
    {"menu.gui.button3.new.input5", "§e请输入使用该组件时所需要的Score分数"},
    {"menu.gui.button3.new.input6", "§e请输入使用该组件时所执行的内容"},
    {"menu.gui.button3.new.dropdown1", "§e请选择组件类型"},
    {"menu.gui.button3.new.dropdown2", "§e请选择按钮类型"},
    {"menu.gui.button3.new.slider", "§b请选择组件权限等级"},
    {"menu.gui.button3.remove.content", "§e§l确定删除组件 §r${customize} §e§l吗"},
    {"menu.gui.button3.remove.yes", "§c确认删除"},
    {"menu.gui.button3.remove.no", "§a取消删除"},
    {"menu.gui.button3.command.line", "§a第${index}行"},
    {"menu.gui.button3.command.addLine", "§b添加一行"},
    {"menu.gui.button3.command.removeLine", "§c删除一行"}, 
    {"tpa.log", "已将玩家 ${player1} 传送到玩家 ${player2}"},
    {"tpa.here", "§e玩家 §b{player} §e邀请你传送到他的位置"},
    {"tpa.there", "§e玩家 §b{player} §e请求传送到你的位置"},
    {"tpa.yes", "§a同意传送"},
    {"tpa.no", "§c拒绝传送"},
    {"tpa.no.tips", "§c玩家 §r${player} §c拒绝了你的请求"},
    {"tpa.gui.title", "§eLOITpa §b-> §a玩家互传"},
    {"tpa.gui.label", "§b功能如下:"},
    {"tpa.gui.label2", "§e请选择玩家:"},
    {"tpa.gui.dropdown", "§b请选择类型:"},
    {"tpa.gui.setting.title", "§eLOITpa §b-> §a个人设置"},
    {"tpa.gui.setting.label", "§b功能如下:"},
    {"tpa.gui.setting.switch1", "§b拒绝所有人的tpa请求"},
    {"shop.log1", "玩家 {player}({uuid}) 新建商店表单 ${menu}"},
    {"shop.log2", "玩家 {player}({uuid}) 删除商店菜单 ${menu}"},
    {"shop.log3", "玩家 {player}({uuid}) 删除商店菜单 ${menu} 的组件 ${customize}"},
    {"shop.log4", "玩家 {player}({uuid}) 设置商店菜单 ${menu} 的默认内容"},
    {"shop.log5", "玩家 {player}({uuid}) 向商店菜单 ${menu} 添加物品组件"},
    {"shop.gui.title", "§e§lLOICollection -> §b服务器商店"},
    {"shop.gui.label", "§b功能如下:"},
    {"shop.gui.button1", "§a新建表单\n§4新建一个商店表单"},
    {"shop.gui.button2", "§b删除表单\n§4删除一个商店表单"},
    {"shop.gui.button3", "§d添加内容\n§4给商店表单添加内容"},
    {"shop.gui.button1.input1", "§e请输入表单ID"},
    {"shop.gui.button1.input2", "§e请输入退出表单时返回内容"},
    {"shop.gui.button1.input3", "§e请输入没有指定称号时返回内容"},
    {"shop.gui.button1.input4", "§e请输入没有足够物品时返回内容"},
    {"shop.gui.button1.input5", "§e请输入没有足够Score时返回内容"},
    {"shop.gui.button1.input6", "§e请输入表单内容"},
    {"shop.gui.button1.input7", "§e请输入表单标题"},
    {"shop.gui.button1.dropdown", "§b请选择表单类型"},
    {"shop.gui.button2.content", "§e§l确定删除商店表单 §r${menu} §e§l吗"},
    {"shop.gui.button2.yes", "§c确认删除"},
    {"shop.gui.button2.no", "§a取消删除"},
    {"shop.gui.button3.label", "§e§l设置表单 §r${menu}"},
    {"shop.gui.button3.setting", "§d设置内容\n§4设置商店表单内容"},
    {"shop.gui.button3.new", "§9添加组件\n§4添加物品组件至表单"},
    {"shop.gui.button3.remove", "§b删除组件\n§4从表单中删除指定组件"},
    {"shop.gui.button3.new.input1", "§e请输入组件标题"},
    {"shop.gui.button3.new.input2", "§e请输入组件图标"},
    {"shop.gui.button3.new.input3", "§e请输入组件介绍"},
    {"shop.gui.button3.new.input4", "§e请输入使用该组件时所需要的Score目标"},
    {"shop.gui.button3.new.input5", "§e请输入使用该组件时所需要的Score分数"},
    {"shop.gui.button3.new.input6", "§e请输入使用该组件时所执行的内容"},
    {"shop.gui.button3.new.input7", "§e请输入组件执行组件文本 - 1"},
    {"shop.gui.button3.new.input8", "§e请输入组件执行组件文本 - 2"},
    {"shop.gui.button3.new.input9", "§e请输入物品组件持续时间"},
    {"shop.gui.button3.new.dropdown", "§e请选择组件类型"},
    {"shop.gui.button3.remove.content", "§e§l确定删除物品组件 §r${customize} §e§l吗"},
    {"shop.gui.button3.remove.yes", "§c确认删除"},
    {"shop.gui.button3.remove.no", "§a取消删除"},
    {"pvp.log1", "玩家 {player}({uuid}) 开启了PVP"},
    {"pvp.log2", "玩家 {player}({uuid}) 关闭了PVP"},
    {"pvp.off1", "§4对方未开启PVP无法造成伤害"},
    {"pvp.off2", "§4你未开启PVP无法造成伤害"},
    {"pvp.gui.title", "§eLOIPvp §b-> §a服务器PVP"},
    {"pvp.gui.label", "§b功能如下:"},
    {"pvp.gui.on", "§4开启PVP"},
    {"pvp.gui.off", "§a关闭PVP"},
    {"wallet.log", "玩家 ${player1} 向玩家 ${player2} 转账: ${money}"},
    {"wallet.showOff", "{player}炫出了自己的钱包: ${money}"},
    {"wallet.tips", "§4您的Score不足，无法转账"},
    {"wallet.gui.title", "§eLOIWallet §b-> §a个人钱包"},
    {"wallet.gui.label", "§b拥有金钱: ${money}\n§e今日税率: ${tax}"},
    {"wallet.gui.transfer", "§a玩家转账"},
    {"wallet.gui.wealth", "§b查看Score"},
    {"wallet.gui.transfer.label", "§b请选择玩家"},
    {"wallet.gui.transfer.input", "§e请输入金额"},
    {"chat.log1", "玩家 {player}({uuid}) 更改称号为 {title}"},
    {"chat.log2", "玩家 {player}({uuid}) 获得称号 ${title}"},
    {"chat.log3", "玩家 {player}({uuid}) 移除称号 ${title}"},
    {"chat.log4", "玩家 {player}({uuid}) 称号 ${title} 已达到预定时间"},
    {"chat.gui.title", "§eLOIChat §b-> §a个人称号"},
    {"chat.gui.label", "§b功能如下:"},
    {"chat.gui.setTitle", "§a设置称号\n§4设置自己的称号显示"},
    {"chat.gui.setTitle.label", "§a当前配戴称号: §f{title}\n§e称号到期时间: §f{title.time}"},
    {"chat.gui.setTitle.dropdown", "§b请选择配戴称号"},
    {"chat.gui.manager.add", "§a添加称号\n§4添加称号给指定玩家"},
    {"chat.gui.manager.remove", "§c删除称号\n§4从玩家上删除指定称号"},
    {"chat.gui.manager.add.label", "§a请选择要添加的玩家"},
    {"chat.gui.manager.add.input1", "§e请输入要添加的称号"},
    {"chat.gui.manager.add.input2", "§d请输入称号存在的时间[/hour](0为永久存在)"},
    {"chat.gui.manager.remove.label", "§a请选择要删除的玩家"},
    {"chat.gui.manager.remove.dropdown", "§e请选择要删除的称号"},
    {"announcement.log", "玩家 {player}({uuid}) 重新编辑了公告"},
    {"announcement.gui.title", "§eLOIAnnounCement §b-> §a公告系统"},
    {"announcement.gui.label", "§b功能如下:"},
    {"announcement.gui.setTitle", "§e设置标题"},
    {"announcement.gui.line", "§a第${index}行"},
    {"announcement.gui.addLine", "§b添加一行"},
    {"announcement.gui.removeLine", "§c删除一行"}, 
    {"announcement.gui.setting.switch1", "§b关闭公告"},
    {"market.log1", "玩家 {player}({uuid}) 购买物品 ${item}"},
    {"market.log2", "玩家 {player}({uuid}) 出售物品 ${item}"},
    {"market.log3", "玩家 {player}({uuid}) 下架物品 ${item}"},
    {"market.gui.title", "§e§lLOIMarket -> §b玩家市场"},
    {"market.gui.label", "§b功能如下:"},
    {"market.gui.sell.introduce", "§b物品介绍: §r${introduce}\n§e物品价格: §r${score}\n§a物品Nbt: §r${nbt}\n§d上传玩家: §r${player}"},
    {"market.gui.sell.sellItem", "§a售出物品\n§4售出自己的手持物品"},
    {"market.gui.sell.sellItemContent", "§b查看售出物品\n§4查看自己售出的物品"},
    {"market.gui.sell.sellItem.dropdown", "§b请选择物品"},
    {"market.gui.sell.sellItem.dropdown.text", "${item} §bx${count}"},
    {"market.gui.sell.sellItem.input1", "§e请输入物品名称"},
    {"market.gui.sell.sellItem.input2", "§e请输入物品图标路径"},
    {"market.gui.sell.sellItem.input3", "§e请输入物品介绍"},
    {"market.gui.sell.sellItem.input4", "§e请输入物品售卖Score价格"},
    {"market.gui.sell.sellItem.tips1", "§a您的物品 §f${item}§a 已售出"},
    {"market.gui.sell.sellItem.tips2", "§a物品 §f${item}§a 已下架"},
    {"market.gui.sell.sellItem.tips3", "§c您没有足够的分数购买这个物品"},
    {"market.gui.sell.sellItem.tips4", "§c您上传的物品已达上限 §r${size}"},
    {"market.gui.sell.sellItemContent.button1", "§c下架物品\n§4下架该物品"},
    {"market.gui.sell.buy.button1", "§e购买物品\n§4购买当前物品"}
};

#endif