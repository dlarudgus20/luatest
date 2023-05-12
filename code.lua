local scene = Entity.new()
scene.rect1 = Entity.new()
scene.rect1:addComponent('transform', Transform.new():move(100, 100):scale(30, 30))
scene.rect1:addComponent('renderer', RectRenderer.new())
scene.rect2 = Entity.new()
scene.rect2:addComponent('transform', Transform.new():move(300, 100):scale(50, 50))
scene.rect2:addComponent('renderer', RectRenderer.new())

local script = Script.new()
function script:onUpdate()
    scene.rect1.components['transform']:move(Input.axis['Horizontal'] * 300, Input.axis['Vertical'] * 300)
end
scene.rect1:addComponent('script', script)

return scene
