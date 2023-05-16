local scene = Entity.new()
scene.stage = Entity.new()
scene.stage.components.transform:scaleBy(500, 600)
scene.stage:addComponent('renderer', RectRenderer.new():setFill(false):setColor(1, 1, 1, 1))

scene.player = Entity.new():setTag('player')
scene.player.components.transform:moveBy(0, -285)
scene.player:addComponent('script', Script.new())
scene.player.rect = Entity.new()
scene.player.rect.components.transform:scaleBy(30, 30)
scene.player.rect:addComponent('rigidBody', RigidBody.new())
scene.player.rect:addComponent('collider', RectCollider.new())
scene.player.rect:addComponent('renderer', RectRenderer.new():setColor(1, 0.5, 0.5, 0.8))
scene.player.deco = Entity.new()
scene.player.deco:addComponent('script', Script.new())
scene.player.deco.rect = Entity.new()
scene.player.deco.rect.components.transform:moveBy(10, 0):scaleBy(5, 5)
scene.player.deco.rect:addComponent('renderer', RectRenderer.new():setColor(1, 1, 1, 1))

scene.rocks = Entity.new()
scene.rocks:addComponent('script', Script.new())
scene.enemies = Entity.new()
scene.bullets = Entity.new()

local function createParticle(tag, parent, radius, leap, period, startX, startY, endY)
    local particle = Entity.new():setTag(tag)
    local script = Script.new()
    particle.components.transform:scaleBy(radius * 2, radius * 2):moveBy(startX, startY)
    particle:addComponent('renderer', RectRenderer.new())
    particle:addComponent('rigidBody', RigidBody.new())
    particle:addComponent('collider', RectCollider.new())
    particle:addComponent('script', script)
    parent:pushChild(particle)

    script.prev = Time.time
    function script:onUpdate()
        local now = Time.time
        if now - script.prev >= period then
            script.prev = now

            local tr = particle.components.transform
            local pos = tr:worldpos()
            pos.y = pos.y + leap
            tr:setworldpos(pos)

            if (endY - pos.y) * leap <= 0 then
                for i, v in ipairs(parent.children) do
                    if v == particle then
                        parent:removeChild(i)
                        break
                    end
                end
            end
        end
    end

    return particle
end

local playerScript = scene.player.components.script
playerScript.nextFire = Time.time
playerScript.remainFire = 20
function playerScript:onUpdate()
    local tr = scene.player.components.transform
    local pos = tr:worldpos()
    pos.x = Util.clamp(pos.x + Input.axis['Horizontal'] * 300, -235, 235)
    pos.y = Util.clamp(pos.y + Input.axis['Vertical'] * 300, -285, -220)
    tr:setworldpos(pos)

    local now = Time.time
    if now >= self.nextFire and self.remainFire == 0 then
        self.remainFire = 20
    end
    if Input.button['Fire1'] and now >= self.nextFire then
        self.remainFire = self.remainFire - 1
        if self.remainFire > 0 then
            self.nextFire = now + 0.075
        else
            self.nextFire = now + 2.75
        end
        local radius = 2.5
        createParticle('bullet', scene.bullets, radius, 10, 0.02, pos.x, pos.y + 15 + radius, 300 - radius)
    end
end
function playerScript:onCollide(other)
    if other.tag == 'rock' then
        -- TODO
    end
end

local decoScript = scene.player.deco.components.script
function decoScript:onUpdate(deco)
    local tr = deco.components.transform
    tr.rotate = tr.rotate + Time.deltaTime * math.pi
end

local rocksScript = scene.rocks.components.script
rocksScript.nextSpawn = Time.time + 0.5
function rocksScript:onUpdate()
    local now = Time.time
    if now >= self.nextSpawn then
        self.nextSpawn = now + 0.5
        local radius = 7.5
        local x = math.random(-(250 - radius), 250 - radius)
        local rock = createParticle('rock', scene.rocks, radius, -10, 0.05, x, 300 - radius, -(300 - radius))
        function rock.components.script:onCollide(other)
            if other.tag == 'bullet' then
                -- TODO
            end
        end
    end
end

return scene
