local scene = Entity.new()
scene.rect1 = Entity.new()
scene.rect1:addComponent('transform', Transform.new():scale(30, 30):move(-100, 0))
scene.rect1:addComponent('renderer', RectRenderer.new():setColor(1, 0.5, 0.5, 0.8))
scene[0] = Entity.new()
scene[0]:addComponent('transform', Transform.new():scale(50, 50):move(100, 0))
scene[0]:addComponent('renderer', RectRenderer.new())

local script = Script.new()
function script:onUpdate()
    scene.rect1.components['transform']:move(Input.axis['Horizontal'] * 300, Input.axis['Vertical'] * 300)
end
scene.rect1:addComponent('script', script)

return scene
