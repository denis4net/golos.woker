const EOSTest = require('eosio.test')

const eosTest = new EOSTest()

beforeAll(async (done) => {
  console.log('init')
  await eosTest.init()
  done()
})

it('tests golos.worker contract', async (done) => {
  const appName = 'app.sample'

  console.log('build contract')
  await eosTest.make('contracts/golos.worker')

  console.log('create test account')
  await eosTest.newAccount('user1')

  console.log("create a delgate's accounts")
  await eosTest.newAccount('delegate1')
  await eosTest.newAccount('delegate2')

  console.log('create a golos.worker account')
  await eosTest.newAccount('golos.worker')

  console.log('create an app.sample account')
  await eosTest.newAccount(appName)

  console.log('deploy contract')
  const contract = await eosTest.deploy('golos.worker', 'contracts/golos.worker/golos.worker.wasm', 'contracts/golos.worker/golos.worker.abi')

  console.log('create')
  await contract.createpool(appName, appName, {authorization: appName})
  console.log(await eosTest.eos.getTableRows(true, 'golos.worker', appName, 'states', ));

  console.log('addpropos')
  console.log(await eosTest.eos.getTableRows(true, 'golos.worker', appName, 'proposals', ));

  const proposals = [
    {id: 0, title: "Proposal #1", text: "Let's create worker's pool", user: "user1"},
    {id: 1, title: "Proposal #2", text: "Let's create golos fork", user: "delegate1"}
  ]

  const comments =  [
    {id: 0, user: 'delegate1', text: "Let's do it!"},
    {id: 1, user: 'delegate2', text: 'Noooo!'}
  ]

  for (let proposal of proposals) {
    console.log(proposal)
    await contract.addpropos(appName, proposal.id, proposal.user, proposal.title, proposal.text, {authorization: proposal.user})
    await contract.upvote(appName, proposal.id, 'delegate1', {authorization: 'delegate1'})
    await contract.downvote(appName, proposal.id, 'delegate2', {authorization: 'delegate2'})

    for (let comment of comments) {
      console.log('addcomment', comment)
      await contract.addcomment(appName, proposal.id, comment.id, comment.user, comment.text, {authorization: comment.user})

    }
  }

  console.log('proposals table', await eosTest.eos.getTableRows(true, 'golos.worker', appName, 'proposals', ));

  for (let proposal of proposals) {
    for (let comment of comments) {
      console.log('delcomment', proposal, comment)
      await contract.delcomment(appName, proposal.id, comment.id, {authorization: comment.user})
    }

    await contract.delpropos(appName, proposal.id, {authorization: proposal.user})
  }

  done()
}, 300000)
